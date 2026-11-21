#include "fanlink.h"

#include <Arduino.h>
#include <esp_random.h>
#include <string.h>

#include "agarce.h"
#include "describe.h"
#include "fancmd.h"
#include "fanstate.h"
#include "radio.h"
#include "store.h"

namespace fanlink {
namespace {

using Key = store::Key;

// The index the board pairs as: group << 4 | device, so 17 = group 1, device 1. The app's own commands use groups >= 1
// (17, 33), and a fan-light paired at index 17 obeys it; group 0 (index 1) is untested.
constexpr uint8_t kOwnIndex = 17;
uint32_t sendMs = 3000;  // how long one command is advertised: 3 s, as the app sends; DURATION changes it
// Refuse a console send within a second of the last: never flood the fan. A screen tap may replace the command on
// the air at once (the newest wins); the screen allows one send per 400 ms (main.cpp, kTapGapMs).
constexpr uint32_t kMinGapMs = 1000;

Key key;
bool learning = false;
uint32_t appCommands = 0;
char last[64] = "nothing yet";
bool heardAny = false;  // the last distinct app command heard (any ID), to drop its repeats
uint16_t heardSeed = 0;
uint8_t heardTx = 0;
uint32_t lastSendMs = 0;
bool sentOnce = false;
bool heardKeySinceBoot = false;  // an app command with our key's ID heard since boot (needed for a learned key)
uint32_t pairUntilMs = 0;  // non-zero while the pair command is on the air
fanstate::State fanState;    // the fan and light state, as last heard from the app or last sent

// IDs are logged as a fingerprint ("#a1b2"), never as their bytes: enough to tell IDs apart in this board's logs. An
// own ID is only 16 bits, so an unkeyed hash of it could be reversed by trying them all; the hash is keyed with a
// random salt that never leaves the board (NVS), so a fingerprint in a shared log doesn't give the ID away.
uint32_t fingerprintSalt = 0;

void maskId(uint32_t id, char* out, size_t cap) {
  uint32_t h = (id ^ fingerprintSalt) * 2654435761u;
  h ^= h >> 15;
  h *= 2246822519u;
  h ^= fingerprintSalt >> 7;
  h ^= h >> 13;
  h *= 3266489917u;
  snprintf(out, cap, "#%04x", (unsigned)(h >> 16));
}

void save() { store::saveKey(key); }

void observeAndSave(const agarce::Packet& p) {
  fanstate::observe(p, fanState);
  store::saveState(fanState);
}

uint32_t onAirUntilMs = 0;  // when the command on the air ends (for the screen's "Sending…")

// The identity and the next counters (ha-ble-adv's rule: when tx wraps to 0, the restart count goes up).
agarce::Packet nextPacket() {
  agarce::Packet p;
  p.prefix = key.prefix;
  p.id = key.id;
  p.index = key.index;
  p.rem0 = key.rem0;
  p.rem1 = key.rem1;
  p.tx = key.tx;
  p.restart = key.restart;
  agarce::nextSequence(p);  // the whole 28-bit counter, so it never goes backwards
  p.seed = (uint16_t)(1 + esp_random() % 0xFFF5);
  return p;
}

using radio::putBytesOnAir;
using radio::putOnAir;

// Keep the counters a packet used (once per command, not per re-advertisement: NVS wears).
void keepCounters(const agarce::Packet& p) {
  key.tx = p.tx;
  key.restart = p.restart;
  key.rem0 = p.rem0;
  key.rem1 = p.rem1;
  save();
}

bool advertise(const agarce::Packet& p, uint32_t ms) {
  if (!putOnAir(p, ms)) return false;
  keepCounters(p);
  onAirUntilMs = millis() + ms;
  return true;
}

// The pair broadcast is a cycle, every kPairCycleMs: a FRESH pair command (new tx and seed), sent as v4 for
// kPairSliceMs then as v3 for kPairSliceMs (as ha-ble-adv's pairing flow sends both Smart Light codecs, 300 ms each),
// then quiet. One unchanged packet broadcast across the power-up doesn't pair: the receiver relays it once as it
// boots and, it seems, takes every later copy as a repeat. A fresh command every cycle lands inside the pairing window.
constexpr uint32_t kPairSliceMs = 300;
constexpr uint32_t kPairCycleMs = 2000;
enum class PairPhase { Quiet, V4, V3 };
PairPhase pairPhase = PairPhase::Quiet;
agarce::Packet pairPacket;
uint32_t pairCycleStartMs = 0;
uint32_t pairSliceEndMs = 0;  // when the current phase ends
uint32_t pairCycles = 0;

// Advance the pair cycle one phase. Returns false if advertising failed.
bool pairStep(uint32_t now) {
  switch (pairPhase) {
    case PairPhase::Quiet:
      pairPacket = nextPacket();
      pairPacket.prefix = 0x84;
      fancmd::buildPair(pairPacket);
      if (!putOnAir(pairPacket, kPairSliceMs + 50)) return false;
      keepCounters(pairPacket);
      pairCycles++;
      pairCycleStartMs = now;
      pairPhase = PairPhase::V4;
      pairSliceEndMs = now + kPairSliceMs;
      return true;
    case PairPhase::V4:
      pairPacket.prefix = 0x83;
      if (!putOnAir(pairPacket, kPairSliceMs + 50)) return false;
      pairPhase = PairPhase::V3;
      pairSliceEndMs = now + kPairSliceMs;
      return true;
    case PairPhase::V3: {
      radio::stop();
      pairPhase = PairPhase::Quiet;
      pairSliceEndMs = pairCycleStartMs + kPairCycleMs;
      return true;
    }
  }
  return false;
}

// `gap`: enforce the console's one-second gap between sends (the screen's taps replace the command on the air instead)
bool ready(const char* what, bool gap = true) {
  if (!key.valid) {
    Serial.printf("ERR %s: no key (NEWKEY, or LEARN from the app)\n", what);
    return false;
  }
  if (!key.own && !heardKeySinceBoot) {
    // A learned key shares its counters with the app, which may have moved on while the board was off
    Serial.printf("ERR %s: learned key, and the app's counter is unknown since boot: press any button in the app\n",
                  what);
    return false;
  }
  if (gap && sentOnce && millis() - lastSendMs < kMinGapMs) {
    Serial.printf("ERR %s: too soon after the last send\n", what);
    return false;
  }
  return true;
}

}  // namespace

void begin() {
  key = store::loadKey();
  fanState = store::loadState();
  fingerprintSalt = store::fingerprintSalt();
}

void onAdv(const uint8_t* adv, size_t len, uint32_t ms, int rssi) {
  agarce::Packet p;
  agarce::Decode r = agarce::decodeAdv(adv, len, p);
  if (r != agarce::Decode::Ok) {
    // Printed in full (raw=): a packet that doesn't decode isn't one of the board's (those always decode), and its
    // bytes are what offline analysis with other codecs needs (e.g. the receiver's own packet at power-up).
    char diag[128], raw[2 * 31 + 1];
    agarce::diagnose(adv, len, diag, sizeof(diag));
    for (size_t i = 0; i < len && i < 31; i++) snprintf(raw + 2 * i, 3, "%02x", adv[i]);
    raw[2 * (len < 31 ? len : 31)] = '\0';
    Serial.printf("FAN %lu %d undecodable (%s) %s raw=%s\n", (unsigned long)ms, rssi,
                  r == agarce::Decode::BadChecksum ? "checksum" : r == agarce::Decode::GroupPacket ? "group" : "format",
                  diag, raw);
    return;
  }
  // The app repeats each command for ~400 ms; one (seed, tx) is one command
  if (heardAny && p.seed == heardSeed && p.tx == heardTx) return;
  heardAny = true;
  heardSeed = p.seed;
  heardTx = p.tx;
  appCommands++;
  char what[64], idText[12];
  describe::packet(p, what, sizeof(what));
  maskId(p.id, idText, sizeof(idText));
  bool ours = key.valid && p.id == key.id && p.index == key.index;
  Serial.printf("FAN %lu %d v%u tx=%u restart=%u rem=%02x%02x seed=%04x id=%s index=%u%s : %s\n", (unsigned long)ms,
                rssi, p.prefix & 0x0F, p.tx, p.restart, p.rem0, p.rem1, p.seed, idText, p.index,
                ours ? " (key)" : "", what);
  snprintf(last, sizeof(last), "heard: %s", what);
  if (learning && p.cmd != 0x00) {
    key = Key{true, false, p.id, p.index, p.prefix, p.rem0, p.rem1, p.tx, p.restart};
    learning = false;
    ours = true;
    Serial.printf("LEARNED id=%s index=%u (stored in NVS)\n", idText, p.index);
  }
  if (ours) {
    key.tx = p.tx;
    key.restart = p.restart;
    heardKeySinceBoot = true;
    save();
    // Only the app's own commands (a learned key) change the state here. The receiver's relays of the board's own
    // commands also match the key, but the board already observed those when it sent them.
    if (!key.own) observeAndSave(p);
  }
}

void tick() {
  if (pairUntilMs == 0) return;
  uint32_t now = millis();
  if ((int32_t)(now - pairUntilMs) >= 0) {
    pairUntilMs = 0;
    radio::stop();
    pairPhase = PairPhase::Quiet;
    Serial.printf("PAIR done: %lu pair commands sent, now off the air\n", (unsigned long)pairCycles);
    return;
  }
  if ((int32_t)(now - pairSliceEndMs) < 0) return;
  if (!pairStep(now)) {
    pairUntilMs = 0;
    Serial.println("ERR PAIR: advertising failed mid-broadcast; stopped");
  }
}

void newKey() {
  if (key.valid) {
    Serial.println("ERR a key exists: FORGET it first");
    return;
  }
  // ha-ble-adv's pairing flow, which pairs these receivers, draws the ID from 0xFF-0xFFF5 (config_flow.py
  // async_step_pair: randint(0xFF, 0xFFF5)); a receiver ignored pairing with a full 32-bit ID.
  uint32_t id = 0xFF + esp_random() % (0xFFF5 - 0xFF + 1);
  key = Key{true, true, id, kOwnIndex, 0x84, 0x00, 0x10, 0, 0};
  save();
  char idText[12];
  maskId(key.id, idText, sizeof(idText));
  Serial.printf("NEWKEY id=%s index=%u (stored in NVS; PAIR next)\n", idText, key.index);
}

void setPrefix(uint8_t prefix) {
  if (!key.valid) {
    Serial.println("ERR PREFIX: no key");
    return;
  }
  if (prefix != 0x83 && prefix != 0x84) {
    Serial.println("ERR PREFIX: 83 (v3) or 84 (v4)");
    return;
  }
  key.prefix = prefix;
  save();
  Serial.printf("OK prefix %02x\n", prefix);
}

void setDuration(uint32_t ms) {
  if (ms < 200 || ms > 5000) {
    Serial.println("ERR DURATION <ms>: 200 to 5000");
    return;
  }
  sendMs = ms;
  Serial.printf("OK commands are advertised for %lu ms\n", (unsigned long)ms);
}

void setField(const char* args) {
  // SET REM1 <hex> | SET RESTART <0-255>: fields copied blind from ha-ble-adv (rem 00 10, restart 0). The real app's
  // fan commands carried rem 00 50, and ha-ble-adv moved to a random restart count. For trying the variants.
  if (!key.valid) {
    Serial.println("ERR SET: no key");
    return;
  }
  if (strncmp(args, "REM1 ", 5) == 0) {
    key.rem1 = (uint8_t)strtoul(args + 5, nullptr, 16);
  } else if (strncmp(args, "RESTART ", 8) == 0) {
    key.restart = (uint8_t)strtoul(args + 8, nullptr, 10);
  } else if (strncmp(args, "INDEX ", 6) == 0) {
    // The index is group (high nibble) + device (low nibble); a new index needs a new PAIR.
    unsigned long index = strtoul(args + 6, nullptr, 10);
    if (index < 1 || index > 255 || (index & 0x0F) == 0) {
      Serial.println("ERR SET INDEX <1-255>, with a nonzero device (low nibble)");
      return;
    }
    key.index = (uint8_t)index;
  } else {
    Serial.println("ERR SET REM1 <hex> | SET RESTART <0-255> | SET INDEX <1-255>");
    return;
  }
  save();
  Serial.printf("OK rem=%02x%02x restart=%u\n", key.rem0, key.rem1, key.restart);
}

void learn() {
  learning = true;
  Serial.println("OK learning: press any button in the Smart Light app");
}

void forget() {
  key = Key{};
  save();
  Serial.println("OK key erased");
}

void printKey() {
  if (!key.valid) {
    Serial.println("KEY none");
    return;
  }
  char idText[12];
  maskId(key.id, idText, sizeof(idText));
  Serial.printf("KEY %s id=%s index=%u prefix=%02x rem=%02x%02x tx=%u restart=%u\n", key.own ? "own" : "learned",
                idText, key.index, key.prefix, key.rem0, key.rem1, key.tx, key.restart);
}

void pair(uint32_t seconds) {
  // PAIR 1 sends a single pair command (one cycle: v4 then v3), as the app does after power-up. Longer broadcasts
  // send a fresh one every 2 s; a receiver may acknowledge more than one (four flashes rather than two).
  if (seconds < 1 || seconds > 60) {
    Serial.println("ERR PAIR <seconds>: 1 to 60");
    return;
  }
  if (!ready("PAIR")) return;
  uint32_t now = millis();
  pairPhase = PairPhase::Quiet;
  pairCycles = 0;
  if (!pairStep(now)) {
    Serial.println("ERR PAIR: advertising failed, nothing sent");
    return;
  }
  pairUntilMs = now + seconds * 1000;
  if (pairUntilMs == 0) pairUntilMs = 1;
  lastSendMs = now;
  sentOnce = true;
  snprintf(last, sizeof(last), "sent: pair (%lus)", (unsigned long)seconds);
  Serial.printf("PAIRING for %lu s: a fresh pair command every %lu ms (v4 %lu ms, then v3 %lu ms), first tx=%u\n",
                (unsigned long)seconds, (unsigned long)kPairCycleMs, (unsigned long)kPairSliceMs,
                (unsigned long)kPairSliceMs, pairPacket.tx);
}

void raw(const char* args) {
  // RAW <ms> <hex>: one advertisement payload (AD structures, at most 31 bytes) for 20-2000 ms. For packets made off
  // the board (other codecs, generated with ha-ble-adv's encoder offline); the PC sequences and times them.
  char* end = nullptr;
  unsigned long ms = strtoul(args, &end, 10);
  if (end == args || ms < 20 || ms > 2000) {
    Serial.println("ERR RAW <ms 20-2000> <hex payload>");
    return;
  }
  while (*end == ' ') end++;
  uint8_t adv[31];
  size_t len = 0;
  for (const char* h = end; *h != '\0';) {
    if (h[1] == '\0' || len == sizeof(adv)) {
      Serial.println("ERR RAW: payload must be whole hex bytes, at most 31");
      return;
    }
    char byte[3] = {h[0], h[1], '\0'};
    char* bend = nullptr;
    adv[len++] = (uint8_t)strtoul(byte, &bend, 16);
    if (bend != byte + 2) {
      Serial.println("ERR RAW: not hex");
      return;
    }
    h += 2;
  }
  if (len < 3) {
    Serial.println("ERR RAW: payload too short");
    return;
  }
  if (pairUntilMs != 0) {
    Serial.println("ERR RAW: pairing is on the air (STOP first)");
    return;
  }
  if (!putBytesOnAir(adv, len, (uint32_t)ms)) {
    Serial.println("ERR RAW: advertising failed, nothing sent");
    return;
  }
  snprintf(last, sizeof(last), "sent: raw %u bytes", (unsigned)len);
  Serial.printf("RAWSENT %u bytes for %lu ms\n", (unsigned)len, ms);
}

void stop() {
  radio::stop();
  onAirUntilMs = millis();
  pairUntilMs = 0;
  pairPhase = PairPhase::Quiet;
  Serial.println("OK stopped: nothing on the air");
}

namespace {

// One command, from the console (gap enforced) or the screen (replaces whatever is on the air).
bool sendAction(fancmd::Action action, int value, bool gap) {
  if (!ready("SEND", gap)) return false;
  if (pairUntilMs != 0) {
    Serial.println("ERR SEND: pairing is on the air (STOP first)");
    return false;
  }
  // With a learned key the app shares these counters; it doesn't know the board used this tx, so its next command
  // reuses it. Whether the receiver cares is untested (the app's next tap must still work).
  agarce::Packet p = nextPacket();
  if (!fancmd::build(action, value, fanState, p)) {
    Serial.println("ERR SEND: value out of range, nothing sent");
    return false;
  }
  char text[64];
  describe::packet(p, text, sizeof(text));
  if (!advertise(p, sendMs)) {
    Serial.printf("ERR advertising failed, nothing sent : %s\n", text);
    return false;
  }
  lastSendMs = millis();
  sentOnce = true;
  observeAndSave(p);
  snprintf(last, sizeof(last), "sent: %s", text);
  Serial.printf("SENT tx=%u restart=%u seed=%04x for %lu ms : %s\n", p.tx, p.restart, p.seed, (unsigned long)sendMs,
                text);
  return true;
}

}  // namespace

bool send(const char* what) {
  fancmd::Action action;
  int value;
  if (!fancmd::parse(what, action, value)) {
    Serial.printf("ERR send what? LIGHT ON|LIGHT OFF|BRIGHT 1-100|CT 0-100|FAN 1-6|FAN OFF|FAN FWD|FAN REV|ALL OFF, got: %s\n", what);
    return false;
  }
  return sendAction(action, value, true);
}

bool send(fancmd::Action action, int value) { return sendAction(action, value, false); }

const fanstate::State& state() { return fanState; }

void remember(const fanstate::State& s) {
  fanState = s;
  store::saveState(fanState);
}

void sendGroup(const char* args) {
  // GROUP <attr> <p0> <p1> <p2> <last>, all hex: the app's group command. Attribute 7 is
  // the group switch (p2: 1 fan, 2 light; last = group | fan<<6 | light<<7). Attribute 0 (pair/remove) is refused.
  // An optional 6th value is the device TYPE for the packet's first byte (default 8, fan light): the app sends its
  // group light switch as type 0 and its fan switch as type 8.
  unsigned int v[6];
  v[5] = 8;
  int n = sscanf(args, "%x %x %x %x %x %x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]);
  if ((n != 5 && n != 6) || v[0] < 1 || v[0] > 255 || v[1] > 255 || v[2] > 255 || v[3] > 255 || v[4] > 255 ||
      v[5] > 15) {
    Serial.println("ERR GROUP <cmd byte 01-ff> <p0> <p1> <p2> <last byte> [type 0-f] (hex)");
    return;
  }
  if (!ready("GROUP")) return;
  if (pairUntilMs != 0) {
    Serial.println("ERR GROUP: pairing is on the air (STOP first)");
    return;
  }
  agarce::Packet p = nextPacket();
  p.group = true;
  p.cmd = (uint8_t)v[0];
  p.arg0 = (uint8_t)v[1];
  p.arg1 = (uint8_t)v[2];
  p.arg2 = (uint8_t)v[3];
  p.groupByte = (uint8_t)v[4];
  p.prefix = (uint8_t)((v[5] << 4) | (key.prefix & 0x0F));
  if (!advertise(p, sendMs)) {
    Serial.println("ERR GROUP: advertising failed, nothing sent");
    return;
  }
  lastSendMs = millis();
  sentOnce = true;
  observeAndSave(p);  // a group switch changes the light's or the fan's on/off
  snprintf(last, sizeof(last), "sent: group %02x %02x%02x%02x %02x", v[0], v[1], v[2], v[3], v[4]);
  Serial.printf("SENT tx=%u restart=%u seed=%04x for %lu ms : group attr %02x params %02x %02x %02x last %02x type %x\n",
                p.tx, p.restart, p.seed, (unsigned long)sendMs, v[0], v[1], v[2], v[3], v[4], v[5]);
}

void printState() {
  Serial.printf("STATE light=%s br=%u ct=%u fan=%s speed=%u dir=%s\n", fanState.lightOn ? "on" : "off", fanState.br,
                fanState.ct, fanState.fanOn ? "on" : "off", fanState.speed, fanState.reverse ? "reverse" : "forward");
}

Status status() {
  Status s{key.valid, learning, 0, appCommands, {}, false};
  s.onAir = (int32_t)(onAirUntilMs - millis()) > 0 && radio::onAir();
  if (pairUntilMs != 0) {
    int32_t left = (int32_t)(pairUntilMs - millis());
    s.pairingLeftMs = left > 0 ? (uint32_t)left : 0;
  }
  strncpy(s.last, last, sizeof(s.last) - 1);
  return s;
}

}  // namespace fanlink
