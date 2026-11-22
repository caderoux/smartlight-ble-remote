// The agarce codec against ha-ble-adv's own agarce test vectors (tests/codecs/test_agarce.py at commit 484371f8): full
// advertisements captured from the Smart Light app, with the fields ha-ble-adv decodes from them.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "agarce.h"
#include "describe.h"
#include "fancmd.h"
#include "fanstate.h"

namespace {

struct Vector {
  const char* name;
  const char* adv;  // dotted hex, as in test_agarce.py
  uint8_t cmd, arg0, arg1, arg2;
  uint8_t tx;
  uint16_t seed;
};

// All share id 0x100061C8 and index 17.
const Vector kVectors[] = {
    {"light on", "02.01.1A.15.FF.F9.09.84.AE.08.9E.BD.C4.63.3C.CC.AD.E4.15.B2.A0.17.F5.61.29", 0x10, 1, 100, 100, 154, 0x08AE},
    {"light off", "02.01.1A.15.FF.F9.09.84.0B.91.3A.24.5D.C6.99.55.34.41.B0.2A.39.B2.50.F8.11", 0x10, 0, 100, 100, 155, 0x910B},
    {"br 1%", "02.01.19.15.FF.F9.09.84.75.CE.7E.7B.02.B8.E7.0A.6B.3F.FE.11.03.A8.2E.14.11", 0x20, 100, 1, 0, 161, 0xCE75},
    {"fan on 1", "02.01.1A.15.FF.F9.09.84.7A.06.60.B3.CA.B7.E8.C2.A3.30.51.2C.CA.AE.21.80.AB", 0x80, 145, 0, 9, 176, 0x067A},
    {"fan off", "02.01.1A.15.FF.F9.09.84.6D.88.76.3D.44.A0.FF.4C.2D.27.46.32.44.B8.36.BE.17", 0x80, 1, 0, 8, 177, 0x886D},
    {"all off", "02.01.1A.15.FF.F9.09.84.6A.5E.76.EB.92.A7.F8.9A.FB.20.B1.E4.92.D3.31.1F.DD", 0x70, 1, 0, 100, 182, 0x5E6A},
};
const char* kPair = "02.01.1A.15.FF.F9.09.04.67.02.5F.B7.CE.AA.F5.C6.A7.2D.CD.B8.C6.BB.FC.13.9F";

size_t parseHex(const char* dotted, uint8_t* out) {
  size_t n = 0;
  for (const char* p = dotted; *p;) {
    out[n++] = (uint8_t)strtoul(p, nullptr, 16);
    p += 2;
    if (*p == '.') p++;
  }
  return n;
}

void test_vectors_decode() {
  for (const Vector& v : kVectors) {
    uint8_t adv[32];
    size_t len = parseHex(v.adv, adv);
    TEST_ASSERT_EQUAL_MESSAGE(25, len, v.name);  // anti-vacuity: the vector parsed
    agarce::Packet p;
    TEST_ASSERT_TRUE_MESSAGE(agarce::decodeAdv(adv, len, p) == agarce::Decode::Ok, v.name);
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(0x100061C8, p.id, v.name);
    TEST_ASSERT_EQUAL_MESSAGE(17, p.index, v.name);
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(v.cmd, p.cmd, v.name);
    TEST_ASSERT_EQUAL_MESSAGE(v.arg0, p.arg0, v.name);
    TEST_ASSERT_EQUAL_MESSAGE(v.arg1, p.arg1, v.name);
    TEST_ASSERT_EQUAL_MESSAGE(v.arg2, p.arg2, v.name);
    TEST_ASSERT_EQUAL_MESSAGE(v.tx, p.tx, v.name);
    TEST_ASSERT_EQUAL_HEX16_MESSAGE(v.seed, p.seed, v.name);
  }
}

// Decoding then encoding gives back the exact manufacturer data the app sent.
void test_vectors_round_trip() {
  for (const Vector& v : kVectors) {
    uint8_t adv[32], mfg[agarce::kMfgLen];
    size_t len = parseHex(v.adv, adv);
    agarce::Packet p;
    TEST_ASSERT_TRUE(agarce::decodeAdv(adv, len, p) == agarce::Decode::Ok);
    TEST_ASSERT_TRUE(agarce::encodeMfg(p, mfg));
    TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(adv + 5, mfg, agarce::kMfgLen, v.name);
  }
}

// fancmd builds the same light on/off packets as the app, given the app's identity and counters.
void test_build_light_matches_app() {
  for (int i = 0; i < 2; i++) {
    const Vector& v = kVectors[i];
    uint8_t adv[32], mfg[agarce::kMfgLen];
    size_t len = parseHex(v.adv, adv);
    agarce::Packet app, mine;
    TEST_ASSERT_TRUE(agarce::decodeAdv(adv, len, app) == agarce::Decode::Ok);
    mine = app;  // identity, counters and seed
    mine.cmd = mine.arg0 = mine.arg1 = mine.arg2 = 0;
    fanstate::State state;  // defaults: ct 100, br 100, as in the vectors
    TEST_ASSERT_TRUE(fancmd::build(i == 0 ? fancmd::Action::LightOn : fancmd::Action::LightOff, 0, state, mine));
    TEST_ASSERT_TRUE(agarce::encodeMfg(mine, mfg));
    TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(adv + 5, mfg, agarce::kMfgLen, v.name);
  }
}

// ALL OFF is built byte for byte as ha-ble-adv's "all off" vector, given its identity and counters.
void test_build_all_off_matches_vector() {
  const Vector& v = kVectors[5];
  uint8_t adv[32], mfg[agarce::kMfgLen];
  size_t len = parseHex(v.adv, adv);
  agarce::Packet app, mine;
  TEST_ASSERT_TRUE(agarce::decodeAdv(adv, len, app) == agarce::Decode::Ok);
  mine = app;
  mine.cmd = mine.arg0 = mine.arg1 = mine.arg2 = 0;
  fancmd::Action a;
  int speed;
  TEST_ASSERT_TRUE(fancmd::parse("ALL OFF", a, speed));
  fanstate::State state;
  TEST_ASSERT_TRUE(fancmd::build(a, speed, state, mine));
  TEST_ASSERT_TRUE(agarce::encodeMfg(mine, mfg));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(adv + 5, mfg, agarce::kMfgLen);
}

// A group command, byte for byte as the real app sent it (NicoIIT/esphome-components issue 17: group "Light ON",
// room group 1): decoded 84.E9.8A.2C.0A.00.10.C8.61.00.10.07.00.00.06.C1.4D.48.
void test_group_command_matches_app() {
  uint8_t adv[32];
  size_t len = parseHex("02.01.1A.15.FF.F9.09.84.E9.8A.6F.3B.46.24.7B.4E.2F.A3.44.31.46.32.72.62.67", adv);
  TEST_ASSERT_EQUAL(25, len);
  agarce::Packet p;
  p.prefix = 0x84;
  p.seed = 0x8AE9;
  p.tx = 0x2C;
  p.restart = 0x0A;
  p.rem0 = 0x00;
  p.rem1 = 0x10;
  p.id = 0x100061C8;
  p.group = true;
  p.cmd = 0x07;
  p.arg0 = 0x00;
  p.arg1 = 0x00;
  p.arg2 = 0x06;
  p.groupByte = 0xC1;
  uint8_t mfg[agarce::kMfgLen];
  TEST_ASSERT_TRUE(agarce::encodeMfg(p, mfg));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(adv + 5, mfg, agarce::kMfgLen);
  p.cmd = 0x00;  // attribute 0 (pair/remove) is never built as a group command
  TEST_ASSERT_FALSE(agarce::encodeMfg(p, mfg));
}

// The guard can fail: any single flipped bit in the scrambled body or the checksum is refused.
void test_tampered_is_refused() {
  uint8_t adv[32];
  size_t len = parseHex(kVectors[0].adv, adv);
  int refused = 0, tried = 0;
  for (size_t byte = 8; byte < len; byte++) {  // after F9 09 84: seed, body, checksum
    for (int bit = 0; bit < 8; bit++) {
      uint8_t copy[32];
      memcpy(copy, adv, len);
      copy[byte] ^= (uint8_t)(1 << bit);
      agarce::Packet p;
      tried++;
      if (agarce::decodeAdv(copy, len, p) != agarce::Decode::Ok) refused++;
    }
  }
  TEST_ASSERT_EQUAL(tried, refused);
}

// diagnose never shows the ID bytes, and shows the checksums.
void test_diagnose_hides_id() {
  uint8_t adv[32];
  size_t len = parseHex(kVectors[0].adv, adv);
  adv[len - 1] ^= 0x01;  // outer checksum broken
  char text[160];
  agarce::diagnose(adv, len, text, sizeof(text));
  TEST_ASSERT_NOT_NULL(strstr(text, "xx xx xx xx"));
  TEST_ASSERT_NOT_NULL(strstr(text, "outer=28/29"));  // received 0x29^1, computed 0x29
  TEST_ASSERT_NULL(strstr(text, "c8 61 00 10"));      // the vector's ID 0x100061C8, little-endian
}

// ha-ble-adv's PAIR vector decodes to its listed fields (cmd 0x00, args [1,0,0], id 0x100061C8, index 17, tx 146,
// seed 0x0267, prefix restored to 0x84), and fancmd::buildPair with that identity rebuilds the identical bytes.
void test_pair_vector() {
  uint8_t adv[32], mfg[agarce::kMfgLen];
  size_t len = parseHex(kPair, adv);
  TEST_ASSERT_EQUAL(25, len);
  agarce::Packet p;
  TEST_ASSERT_TRUE(agarce::decodeAdv(adv, len, p) == agarce::Decode::Ok);
  TEST_ASSERT_EQUAL_HEX8(0x00, p.cmd);
  TEST_ASSERT_EQUAL(1, p.arg0);
  TEST_ASSERT_EQUAL_HEX32(0x100061C8, p.id);
  TEST_ASSERT_EQUAL(17, p.index);
  TEST_ASSERT_EQUAL(146, p.tx);
  TEST_ASSERT_EQUAL_HEX16(0x0267, p.seed);
  TEST_ASSERT_EQUAL_HEX8(0x84, p.prefix);
  agarce::Packet mine = p;
  mine.cmd = 0x55;  // overwritten by buildPair
  fancmd::buildPair(mine);
  TEST_ASSERT_TRUE(agarce::encodeMfg(mine, mfg));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(adv + 5, mfg, agarce::kMfgLen);
}

// The UNPAIR vector decodes (so the board can report one if it hears it), but can never be built.
void test_unpair_is_never_built() {
  uint8_t adv[32], mfg[agarce::kMfgLen];
  size_t len = parseHex("02.01.1A.15.FF.F9.09.04.8F.0E.B6.BB.C2.42.1D.CA.AB.C5.25.B5.CA.53.14.1F.97", adv);
  agarce::Packet p;
  TEST_ASSERT_TRUE(agarce::decodeAdv(adv, len, p) == agarce::Decode::Ok);
  TEST_ASSERT_EQUAL_HEX8(0x00, p.cmd);
  TEST_ASSERT_EQUAL(0, p.arg0);
  TEST_ASSERT_EQUAL(147, p.tx);
  TEST_ASSERT_EQUAL_HEX16(0x0E8F, p.seed);
  TEST_ASSERT_EQUAL(17, p.index);
  TEST_ASSERT_FALSE(agarce::encodeMfg(p, mfg));
  char text[32];
  describe::packet(p, text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("UNPAIR", text);
}

// ha-ble-adv's bare PAIR encoder vector (manufacturer data only) round-trips byte for byte.
void test_pair_mfg_vector_round_trip() {
  uint8_t mfg[agarce::kMfgLen], again[agarce::kMfgLen];
  TEST_ASSERT_EQUAL(20, parseHex("F9.09.04.DD.10.45.A5.DC.10.4F.D4.B5.97.77.AA.D4.01.46.E1.53", mfg));
  agarce::Packet p;
  TEST_ASSERT_TRUE(agarce::decodeMfg(mfg, sizeof(mfg), p) == agarce::Decode::Ok);
  TEST_ASSERT_EQUAL_HEX8(0x00, p.cmd);
  TEST_ASSERT_EQUAL(1, p.arg0);
  TEST_ASSERT_TRUE(agarce::encodeMfg(p, again));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(mfg, again, agarce::kMfgLen);
}

// Pair packets round-trip their identity across indexes (both nibbles) and both prefixes.
void test_pair_sweep() {
  for (int i = 0; i < 512; i++) {
    agarce::Packet in;
    in.prefix = (i & 1) ? 0x83 : 0x84;
    in.id = 0x9E3779B9u * (uint32_t)(i + 7);
    in.index = (uint8_t)(i >> 1);
    in.tx = (uint8_t)(i * 3);
    in.seed = (uint16_t)(1 + (i * 977u) % 0xFFF5);
    fancmd::buildPair(in);
    uint8_t mfg[agarce::kMfgLen];
    TEST_ASSERT_TRUE(agarce::encodeMfg(in, mfg));
    TEST_ASSERT_EQUAL_HEX8(in.prefix & 0x0F, mfg[2]);  // on the air without the high nibble
    agarce::Packet out;
    TEST_ASSERT_TRUE(agarce::decodeMfg(mfg, sizeof(mfg), out) == agarce::Decode::Ok);
    TEST_ASSERT_EQUAL_HEX8(in.prefix, out.prefix);
    TEST_ASSERT_EQUAL(in.index, out.index);
    TEST_ASSERT_EQUAL_HEX32(in.id, out.id);
    TEST_ASSERT_EQUAL(1, out.arg0);
    TEST_ASSERT_EQUAL_HEX8(0x00, out.cmd);
  }
}

void test_other_adverts_are_not_agarce() {
  uint8_t ibeacon[32];
  size_t len = parseHex("02.01.06.1a.ff.4c.00.02.15.26.86.f3.9c.ba.da.46.58.85.4a.a6.2e.7e.5e.8b.8d.00.01.00.00.0b", ibeacon);
  agarce::Packet p;
  TEST_ASSERT_TRUE(agarce::decodeAdv(ibeacon, len, p) == agarce::Decode::NotAgarce);
  TEST_ASSERT_FALSE(agarce::isAgarceAdv(ibeacon, len));
}

void test_is_agarce_adv() {
  uint8_t adv[32];
  size_t len = parseHex(kVectors[0].adv, adv);
  TEST_ASSERT_TRUE(agarce::isAgarceAdv(adv, len));
  len = parseHex(kPair, adv);
  TEST_ASSERT_TRUE(agarce::isAgarceAdv(adv, len));
  adv[4] = 0xFE;  // not manufacturer data any more
  TEST_ASSERT_FALSE(agarce::isAgarceAdv(adv, len));
  TEST_ASSERT_FALSE(agarce::isAgarceAdv(adv, 0));
}

// Every encoded packet round-trips, across ids, indexes, counters and seeds (not just the vectors' identity).
void test_encode_decode_sweep() {
  agarce::Packet in;
  int checked = 0;
  for (uint32_t i = 0; i < 2000; i++) {
    in.prefix = (i & 1) ? 0x83 : 0x84;
    in.id = 0x9E3779B9u * (i + 1);
    in.index = (uint8_t)(i * 7);
    in.tx = (uint8_t)i;
    in.restart = (uint8_t)(i >> 3);
    in.seed = (uint16_t)(1 + (i * 40503u) % 0xFFF5);
    in.cmd = (uint8_t)(0x10 + 0x10 * (i % 15));
    in.arg0 = (uint8_t)(i * 3);
    in.arg1 = (uint8_t)(i * 5);
    in.arg2 = (uint8_t)(i * 11);
    uint8_t adv[31];
    size_t len = agarce::encodeAdv(in, adv, sizeof(adv));
    TEST_ASSERT_EQUAL(25, len);
    agarce::Packet out;
    TEST_ASSERT_TRUE(agarce::decodeAdv(adv, len, out) == agarce::Decode::Ok);
    TEST_ASSERT_EQUAL_HEX32(in.id, out.id);
    TEST_ASSERT_EQUAL(in.index, out.index);
    TEST_ASSERT_EQUAL(in.tx, out.tx);
    TEST_ASSERT_EQUAL(in.restart, out.restart);
    TEST_ASSERT_EQUAL_HEX16(in.seed, out.seed);
    TEST_ASSERT_EQUAL_HEX8(in.cmd, out.cmd);
    TEST_ASSERT_EQUAL(in.arg0, out.arg0);
    TEST_ASSERT_EQUAL(in.arg1, out.arg1);
    TEST_ASSERT_EQUAL(in.arg2, out.arg2);
    TEST_ASSERT_EQUAL_HEX8(in.prefix, out.prefix);
    checked++;
  }
  TEST_ASSERT_EQUAL(2000, checked);
}

void test_fan_commands() {
  agarce::Packet p;
  fanstate::State s;
  TEST_ASSERT_TRUE(fancmd::build(fancmd::Action::FanSpeed, 3, s, p));
  TEST_ASSERT_EQUAL_HEX8(0x80, p.cmd);
  TEST_ASSERT_EQUAL_HEX8(0x83, p.arg0);
  TEST_ASSERT_EQUAL_HEX8(0x09, p.arg2);
  TEST_ASSERT_FALSE(fancmd::build(fancmd::Action::FanSpeed, 0, s, p));
  TEST_ASSERT_FALSE(fancmd::build(fancmd::Action::FanSpeed, 7, s, p));
}

// The state heard from the app is carried: a reversed fan stays reversed, a dimmed light stays dimmed.
void test_state_is_carried() {
  uint8_t adv[32];
  agarce::Packet app, p;
  fanstate::State s;
  // The app's "fan on, speed 1" vector is reversed (0x91)
  TEST_ASSERT_TRUE(agarce::decodeAdv(adv, parseHex(kVectors[3].adv, adv), app) == agarce::Decode::Ok);
  fanstate::observe(app, s);
  TEST_ASSERT_TRUE(s.reverse);
  TEST_ASSERT_TRUE(fancmd::build(fancmd::Action::FanSpeed, 4, s, p));
  TEST_ASSERT_EQUAL_HEX8(0x94, p.arg0);  // on | reverse | 4
  // Then the app's "fan off" vector (forward, speed 1, only "on" changed) is reproduced from that state
  TEST_ASSERT_TRUE(agarce::decodeAdv(adv, parseHex(kVectors[4].adv, adv), app) == agarce::Decode::Ok);
  fanstate::observe(app, s);
  TEST_ASSERT_FALSE(s.reverse);
  TEST_ASSERT_TRUE(fancmd::build(fancmd::Action::FanOff, 0, s, p));
  TEST_ASSERT_EQUAL_HEX8(app.arg0, p.arg0);
  TEST_ASSERT_EQUAL_HEX8(app.arg2, p.arg2);
  // Brightness 1% heard: LIGHT ON keeps it
  TEST_ASSERT_TRUE(agarce::decodeAdv(adv, parseHex(kVectors[2].adv, adv), app) == agarce::Decode::Ok);
  fanstate::observe(app, s);
  TEST_ASSERT_TRUE(fancmd::build(fancmd::Action::LightOn, 0, s, p));
  TEST_ASSERT_EQUAL(100, p.arg1);
  TEST_ASSERT_EQUAL(1, p.arg2);
}

// Build `action` from `state` with a vector's identity and counters, and compare with the vector's bytes.
void check_build_matches(const char* name, const char* dotted, fancmd::Action action, int value,
                         const fanstate::State& state) {
  uint8_t adv[32], mfg[agarce::kMfgLen];
  size_t len = parseHex(dotted, adv);
  TEST_ASSERT_EQUAL_MESSAGE(25, len, name);
  agarce::Packet app, mine;
  TEST_ASSERT_TRUE_MESSAGE(agarce::decodeAdv(adv, len, app) == agarce::Decode::Ok, name);
  mine = app;
  mine.cmd = mine.arg0 = mine.arg1 = mine.arg2 = 0;
  TEST_ASSERT_TRUE_MESSAGE(fancmd::build(action, value, state, mine), name);
  TEST_ASSERT_TRUE_MESSAGE(agarce::encodeMfg(mine, mfg), name);
  TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(adv + 5, mfg, agarce::kMfgLen, name);
}

// Direction, brightness and colour temperature, byte for byte as the app sent them (ha-ble-adv's vectors)
void test_build_new_controls_match_app() {
  fanstate::State fan1;  // fan on at speed 1, forward: the state before the app's "FAN DIR Reverse"
  fan1.fanOn = true;
  fan1.speed = 1;
  check_build_matches("fan dir reverse",
                      "02.01.1A.15.FF.F9.09.84.C1.1D.D8.A8.D1.0C.53.D9.B8.8B.EA.37.D1.1E.9A.A7.7F",
                      fancmd::Action::FanReverse, 0, fan1);
  fanstate::State cold;  // ct 100 (cold), as the brightness vectors carry
  check_build_matches("br 1%", kVectors[2].adv, fancmd::Action::Brightness, 1, cold);
  check_build_matches("br 100%", "02.01.19.15.FF.F9.09.84.A4.2D.AA.98.E1.69.36.E9.88.EE.2F.F2.85.79.FF.6D.01",
                      fancmd::Action::Brightness, 100, cold);
  fanstate::State full;  // brightness 100
  check_build_matches("cold", "02.01.19.15.FF.F9.09.84.EB.BD.EB.08.71.26.79.79.18.A1.60.62.15.36.B0.F3.11",
                      fancmd::Action::ColourTemp, 100, full);
  check_build_matches("warm", "02.01.19.15.FF.F9.09.84.99.19.9C.AC.D5.54.0B.DD.BC.D3.12.A2.B1.44.C2.30.B9",
                      fancmd::Action::ColourTemp, 0, full);
}

void test_new_controls_ranges_and_state() {
  fanstate::State s;
  agarce::Packet p;
  TEST_ASSERT_FALSE(fancmd::build(fancmd::Action::Brightness, 0, s, p));
  TEST_ASSERT_FALSE(fancmd::build(fancmd::Action::Brightness, 101, s, p));
  TEST_ASSERT_FALSE(fancmd::build(fancmd::Action::ColourTemp, -1, s, p));
  TEST_ASSERT_FALSE(fancmd::build(fancmd::Action::ColourTemp, 101, s, p));
  // Forward from reverse while the fan is off keeps it off and keeps its speed
  s.reverse = true;
  s.speed = 4;
  TEST_ASSERT_TRUE(fancmd::build(fancmd::Action::FanForward, 0, s, p));
  TEST_ASSERT_EQUAL_HEX8(0x04, p.arg0);
  TEST_ASSERT_EQUAL_HEX8(0x02, p.arg2);
  // The state follows what's sent
  fanstate::observe(p, s);
  TEST_ASSERT_FALSE(s.reverse);
  TEST_ASSERT_TRUE(fancmd::build(fancmd::Action::Brightness, 40, s, p));
  fanstate::observe(p, s);
  TEST_ASSERT_EQUAL(40, s.br);
  TEST_ASSERT_TRUE(fancmd::build(fancmd::Action::ColourTemp, 30, s, p));
  fanstate::observe(p, s);
  TEST_ASSERT_EQUAL(30, s.ct);
  TEST_ASSERT_EQUAL(40, s.br);  // colour keeps brightness
  TEST_ASSERT_TRUE(fancmd::build(fancmd::Action::LightOff, 0, s, p));
  TEST_ASSERT_EQUAL(30, p.arg1);  // light off carries ct and br
  TEST_ASSERT_EQUAL(40, p.arg2);
  fanstate::observe(p, s);
  TEST_ASSERT_FALSE(s.lightOn);
  TEST_ASSERT_TRUE(fancmd::build(fancmd::Action::FanSpeed, 2, s, p));
  fanstate::observe(p, s);
  TEST_ASSERT_TRUE(s.fanOn);
  TEST_ASSERT_EQUAL(2, s.speed);
}

void test_parse() {
  fancmd::Action a;
  int s;
  TEST_ASSERT_TRUE(fancmd::parse("LIGHT ON", a, s));
  TEST_ASSERT_TRUE(a == fancmd::Action::LightOn);
  TEST_ASSERT_TRUE(fancmd::parse("FAN 6", a, s));
  TEST_ASSERT_TRUE(a == fancmd::Action::FanSpeed);
  TEST_ASSERT_EQUAL(6, s);
  TEST_ASSERT_FALSE(fancmd::parse("FAN 7", a, s));
  TEST_ASSERT_FALSE(fancmd::parse("FAN 0", a, s));
  TEST_ASSERT_FALSE(fancmd::parse("FAN 12", a, s));
  TEST_ASSERT_FALSE(fancmd::parse("PAIR", a, s));
  TEST_ASSERT_TRUE(fancmd::parse("FAN REV", a, s));
  TEST_ASSERT_TRUE(a == fancmd::Action::FanReverse);
  TEST_ASSERT_TRUE(fancmd::parse("FAN FWD", a, s));
  TEST_ASSERT_TRUE(a == fancmd::Action::FanForward);
  TEST_ASSERT_TRUE(fancmd::parse("BRIGHT 100", a, s));
  TEST_ASSERT_TRUE(a == fancmd::Action::Brightness);
  TEST_ASSERT_EQUAL(100, s);
  TEST_ASSERT_FALSE(fancmd::parse("BRIGHT 0", a, s));
  TEST_ASSERT_FALSE(fancmd::parse("BRIGHT 101", a, s));
  TEST_ASSERT_FALSE(fancmd::parse("BRIGHT 5x", a, s));
  TEST_ASSERT_FALSE(fancmd::parse("BRIGHT ", a, s));
  TEST_ASSERT_TRUE(fancmd::parse("CT 0", a, s));
  TEST_ASSERT_TRUE(a == fancmd::Action::ColourTemp);
  TEST_ASSERT_EQUAL(0, s);
  TEST_ASSERT_FALSE(fancmd::parse("CT 101", a, s));
  // Only plain digits: no sign, extra spaces, leading zeros or tabs
  TEST_ASSERT_FALSE(fancmd::parse("FAN  3", a, s));
  TEST_ASSERT_FALSE(fancmd::parse("FAN +3", a, s));
  TEST_ASSERT_FALSE(fancmd::parse("CT -0", a, s));
  TEST_ASSERT_FALSE(fancmd::parse("BRIGHT 007", a, s));
  TEST_ASSERT_FALSE(fancmd::parse("FAN \t3", a, s));
  TEST_ASSERT_FALSE(fancmd::parse("BRIGHT 1000", a, s));
}

// Group switches and "all on/off" keep the state honest
void test_state_from_group_and_all() {
  fanstate::State s;
  agarce::Packet g;
  g.group = true;
  g.cmd = 0x07;
  g.arg2 = 0x02;       // the light
  g.groupByte = 0x01;  // group 1, light off, fan off
  s.fanOn = true;
  fanstate::observe(g, s);
  TEST_ASSERT_FALSE(s.lightOn);
  TEST_ASSERT_TRUE(s.fanOn);  // not targeted: unchanged
  g.arg2 = 0x01;              // the fan
  g.groupByte = 0x41;         // fan on
  s.fanOn = false;
  fanstate::observe(g, s);
  TEST_ASSERT_TRUE(s.fanOn);
  TEST_ASSERT_FALSE(s.lightOn);
  agarce::Packet all;
  all.cmd = 0x70;
  all.arg0 = 192;  // all on
  fanstate::observe(all, s);
  TEST_ASSERT_TRUE(s.lightOn);
  TEST_ASSERT_TRUE(s.fanOn);
  all.arg0 = 1;  // all off
  fanstate::observe(all, s);
  TEST_ASSERT_FALSE(s.lightOn);
  TEST_ASSERT_FALSE(s.fanOn);
}

void test_describe() {
  agarce::Packet p;
  p.cmd = 0x80;
  p.arg0 = 145;
  p.arg2 = 9;
  char text[96];
  describe::packet(p, text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("fan on speed 1 reverse (changed: speed on)", text);
}

// The sequence is one 28-bit counter: tx carries into restart, restart into rem0, rem0 into rem1's low nibble; the
// "order" nibble (rem1's high) never changes.
void test_next_sequence_carries() {
  agarce::Packet p;
  p.tx = 41;
  p.restart = 3;
  p.rem0 = 0;
  p.rem1 = 0x10;
  agarce::nextSequence(p);
  TEST_ASSERT_EQUAL(42, p.tx);
  TEST_ASSERT_EQUAL(3, p.restart);
  p.tx = 255;
  agarce::nextSequence(p);  // the wrap seen on the fan: 255 -> 0, restart + 1
  TEST_ASSERT_EQUAL(0, p.tx);
  TEST_ASSERT_EQUAL(4, p.restart);
  TEST_ASSERT_EQUAL(0, p.rem0);
  p.tx = 255;
  p.restart = 255;
  agarce::nextSequence(p);  // restart wraps into rem0, so the counter never goes backwards
  TEST_ASSERT_EQUAL(0, p.tx);
  TEST_ASSERT_EQUAL(0, p.restart);
  TEST_ASSERT_EQUAL(1, p.rem0);
  TEST_ASSERT_EQUAL_HEX8(0x10, p.rem1);
  p.tx = 255;
  p.restart = 255;
  p.rem0 = 255;
  agarce::nextSequence(p);  // rem0 wraps into rem1's low nibble; the order nibble stays
  TEST_ASSERT_EQUAL(0, p.rem0);
  TEST_ASSERT_EQUAL_HEX8(0x11, p.rem1);
}

agarce::Packet atSequence(uint32_t n, uint8_t order = 0x10) {
  agarce::Packet p;
  p.tx = (uint8_t)n;
  p.restart = (uint8_t)(n >> 8);
  p.rem0 = (uint8_t)(n >> 16);
  p.rem1 = (uint8_t)(order | ((n >> 24) & 0x0F));
  return p;
}

void test_sequence_is_the_28_bit_counter() {
  TEST_ASSERT_EQUAL_HEX32(0x0, agarce::sequence(atSequence(0)));
  TEST_ASSERT_EQUAL_HEX32(0x0ABCDEF1, agarce::sequence(atSequence(0x0ABCDEF1)));
  TEST_ASSERT_EQUAL_HEX32(0x0ABCDEF1, agarce::sequence(atSequence(0x0ABCDEF1, 0xF0)));  // the order nibble isn't in it
  agarce::Packet p = atSequence(0x0FFFFFFF);
  agarce::nextSequence(p);  // the counter's own wrap
  TEST_ASSERT_EQUAL_HEX32(0x0, agarce::sequence(p));
  TEST_ASSERT_EQUAL_HEX8(0x10, p.rem1);
  p = atSequence(0x26 | (38 << 8));  // tx 38, restart 38 (as on the fan): the step carries through every byte
  agarce::nextSequence(p);
  TEST_ASSERT_EQUAL_HEX32(0x2627, agarce::sequence(p));
}

void test_newer_follows_the_counter_across_the_wrap() {
  TEST_ASSERT_TRUE(agarce::newer(atSequence(56), atSequence(55)));
  TEST_ASSERT_FALSE(agarce::newer(atSequence(55), atSequence(56)));  // older: a late relay
  TEST_ASSERT_FALSE(agarce::newer(atSequence(56), atSequence(56)));  // equal: our own command's relay
  TEST_ASSERT_TRUE(agarce::newer(atSequence(0x100), atSequence(0xFF)));          // tx wraps into restart
  TEST_ASSERT_TRUE(agarce::newer(atSequence(0x10000), atSequence(0xFFFF)));      // restart wraps into rem0
  TEST_ASSERT_TRUE(agarce::newer(atSequence(0x1000000), atSequence(0xFFFFFF)));  // rem0 into rem1's low nibble
  TEST_ASSERT_TRUE(agarce::newer(atSequence(0), atSequence(0x0FFFFFFF)));        // the 28-bit wrap
  TEST_ASSERT_FALSE(agarce::newer(atSequence(0x0FFFFFFF), atSequence(0)));
  TEST_ASSERT_TRUE(agarce::newer(atSequence(40, 0x10), atSequence(39, 0xF0)));  // the order nibble is ignored
  // The halfway point: never newer both ways
  TEST_ASSERT_TRUE(agarce::newer(atSequence(0x07FFFFFF), atSequence(0)));
  TEST_ASSERT_FALSE(agarce::newer(atSequence(0x08000000), atSequence(0)));
  TEST_ASSERT_FALSE(agarce::newer(atSequence(0), atSequence(0x08000000)));
  TEST_ASSERT_TRUE(agarce::newer(atSequence(0), atSequence(0x08000001)));
}

}  // namespace

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_vectors_decode);
  RUN_TEST(test_vectors_round_trip);
  RUN_TEST(test_build_light_matches_app);
  RUN_TEST(test_build_all_off_matches_vector);
  RUN_TEST(test_group_command_matches_app);
  RUN_TEST(test_tampered_is_refused);
  RUN_TEST(test_diagnose_hides_id);
  RUN_TEST(test_pair_vector);
  RUN_TEST(test_unpair_is_never_built);
  RUN_TEST(test_pair_mfg_vector_round_trip);
  RUN_TEST(test_pair_sweep);
  RUN_TEST(test_other_adverts_are_not_agarce);
  RUN_TEST(test_is_agarce_adv);
  RUN_TEST(test_encode_decode_sweep);
  RUN_TEST(test_fan_commands);
  RUN_TEST(test_state_is_carried);
  RUN_TEST(test_build_new_controls_match_app);
  RUN_TEST(test_new_controls_ranges_and_state);
  RUN_TEST(test_parse);
  RUN_TEST(test_state_from_group_and_all);
  RUN_TEST(test_describe);
  RUN_TEST(test_next_sequence_carries);
  RUN_TEST(test_sequence_is_the_28_bit_counter);
  RUN_TEST(test_newer_follows_the_counter_across_the_wrap);
  return UNITY_END();
}
