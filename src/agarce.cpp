#include "agarce.h"

#include <stdio.h>
#include <string.h>

namespace agarce {
namespace {

constexpr uint8_t kMatrix[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0x5A, 0xA5, 0xA5, 0x5A};
constexpr size_t kBodyLen = 14;  // the scrambled bytes, inner checksum included

void crypt(uint8_t* buf, size_t len, uint8_t seedLo, uint8_t seedHi) {
  const uint8_t pivot[4] = {seedLo, seedHi, seedHi, seedLo};
  for (size_t i = 0; i < len; i++) buf[i] ^= pivot[i % 4] ^ kMatrix[i % 8];
}

uint8_t sum(const uint8_t* buf, size_t len) {
  uint8_t s = 0;
  for (size_t i = 0; i < len; i++) s = (uint8_t)(s + buf[i]);
  return s;
}

}  // namespace

Decode decodeMfg(const uint8_t* mfg, size_t len, Packet& out) {
  if (len != kMfgLen || mfg[0] != 0xF9 || mfg[1] != 0x09) return Decode::NotAgarce;
  const uint8_t* b = mfg + 2;  // prefix, seed lo, seed hi, 14 scrambled, outer checksum
  // On the air, pair/unpair packets carry the prefix without its high nibble (0x03/0x04); the nibble travels in arg1.
  if (b[0] != 0x83 && b[0] != 0x84 && b[0] != 0x03 && b[0] != 0x04) return Decode::NotAgarce;
  if (sum(b, 17) != b[17]) return Decode::BadChecksum;
  uint8_t d[kBodyLen];
  memcpy(d, b + 3, kBodyLen);
  crypt(d, kBodyLen, b[1], b[2]);
  if (sum(d, 13) != d[13]) return Decode::BadChecksum;
  out.seed = (uint16_t)(b[1] | (b[2] << 8));
  out.tx = d[0];
  out.restart = d[1];
  out.rem0 = d[2];
  out.rem1 = d[3];
  out.id = (uint32_t)d[4] | ((uint32_t)d[5] << 8) | ((uint32_t)d[6] << 16) | ((uint32_t)d[7] << 24);
  out.cmd = d[8] & 0xF0;
  out.arg0 = d[9];
  if (out.cmd == 0x00) {
    // Pair (arg0 1) / unpair (arg0 0): arg1 holds the prefix's high nibble, arg2 the index's low nibble, and the
    // index byte 0xC0 + its high nibble. A zero index byte is a group command, which ha-ble-adv excludes too.
    if (d[12] == 0x00) return Decode::GroupPacket;
    out.prefix = (uint8_t)(b[0] | ((d[10] & 0x0F) << 4));
    out.index = (uint8_t)(((d[12] & 0x0F) << 4) + d[11]);
    out.arg1 = 0;
    out.arg2 = 0;
  } else {
    out.prefix = b[0];
    out.index = (uint8_t)(((d[12] & 0x0F) << 4) | (d[8] & 0x0F));
    out.arg1 = d[10];
    out.arg2 = d[11];
  }
  if (out.prefix != 0x83 && out.prefix != 0x84) return Decode::NotAgarce;
  return Decode::Ok;
}

namespace {

// The manufacturer data (AD type 0xFF) in an advertisement payload, or nullptr.
const uint8_t* findMfg(const uint8_t* adv, size_t len, size_t& mfgLen) {
  for (size_t i = 0; i + 1 < len;) {
    size_t adLen = adv[i];
    if (adLen == 0 || i + 1 + adLen > len) return nullptr;
    if (adv[i + 1] == 0xFF) {
      mfgLen = adLen - 1;
      return adv + i + 2;
    }
    i += 1 + adLen;
  }
  return nullptr;
}

}  // namespace

bool isAgarceAdv(const uint8_t* adv, size_t len) {
  size_t mfgLen = 0;
  const uint8_t* m = findMfg(adv, len, mfgLen);
  return m != nullptr && mfgLen == kMfgLen && m[0] == 0xF9 && m[1] == 0x09 &&
         (m[2] == 0x83 || m[2] == 0x84 || m[2] == 0x03 || m[2] == 0x04);
}

Decode decodeAdv(const uint8_t* adv, size_t len, Packet& out) {
  size_t mfgLen = 0;
  const uint8_t* m = findMfg(adv, len, mfgLen);
  return m == nullptr ? Decode::NotAgarce : decodeMfg(m, mfgLen, out);
}

void diagnose(const uint8_t* adv, size_t len, char* out, size_t cap) {
  size_t mfgLen = 0;
  const uint8_t* m = findMfg(adv, len, mfgLen);
  if (m == nullptr || mfgLen != kMfgLen || m[0] != 0xF9 || m[1] != 0x09) {
    snprintf(out, cap, "not 20 bytes of F9 09 data (%u)", (unsigned)mfgLen);
    return;
  }
  const uint8_t* b = m + 2;
  uint8_t d[kBodyLen];
  memcpy(d, b + 3, kBodyLen);
  crypt(d, kBodyLen, b[1], b[2]);
  int n = snprintf(out, cap, "p=%02x outer=%02x/%02x inner=%02x/%02x d=", b[0], b[17], sum(b, 17), d[13], sum(d, 13));
  for (size_t i = 0; i < 13 && n > 0 && (size_t)n < cap; i++) {
    if (i >= 4 && i <= 7) {
      n += snprintf(out + n, cap - n, "xx ");
    } else {
      n += snprintf(out + n, cap - n, "%02x ", d[i]);
    }
  }
}

bool encodeMfg(const Packet& p, uint8_t out[kMfgLen]) {
  // Group commands: attribute 1-15 (attribute 0 is pair/remove, never built this way)
  if (p.group && p.cmd == 0x00) return false;
  const bool pair = !p.group && (p.cmd & 0xF0) == 0x00;
  // Unpair (cmd 0x00, arg0 0) is never built: nothing here may make the fan forget a controller.
  if (pair && p.arg0 != 1) return false;
  uint8_t d[kBodyLen] = {p.tx, p.restart, p.rem0, p.rem1,
                         (uint8_t)p.id, (uint8_t)(p.id >> 8), (uint8_t)(p.id >> 16), (uint8_t)(p.id >> 24),
                         0, p.arg0, 0, 0, 0, 0};
  uint8_t wirePrefix = p.prefix;
  if (pair) {
    d[8] = 0x00;
    d[10] = (uint8_t)((p.prefix & 0xF0) >> 4);
    d[11] = (uint8_t)(p.index & 0x0F);
    d[12] = (uint8_t)(0xC0 + ((p.index >> 4) & 0x0F));
    wirePrefix = (uint8_t)(p.prefix & 0x0F);
  } else if (p.group) {
    d[8] = p.cmd;
    d[10] = p.arg1;
    d[11] = p.arg2;
    d[12] = p.groupByte;
  } else {
    d[8] = (uint8_t)((p.cmd & 0xF0) | (p.index & 0x0F));
    d[10] = p.arg1;
    d[11] = p.arg2;
    d[12] = (uint8_t)((p.index >> 4) & 0x0F);
  }
  d[13] = sum(d, 13);
  uint8_t lo = (uint8_t)p.seed, hi = (uint8_t)(p.seed >> 8);
  crypt(d, kBodyLen, lo, hi);
  out[0] = 0xF9;
  out[1] = 0x09;
  uint8_t* b = out + 2;
  b[0] = wirePrefix;
  b[1] = lo;
  b[2] = hi;
  memcpy(b + 3, d, kBodyLen);
  b[17] = sum(b, 17);
  return true;
}

size_t encodeAdv(const Packet& p, uint8_t* out, size_t cap) {
  const size_t need = 3 + 2 + kMfgLen;
  if (cap < need) return 0;
  out[0] = 0x02;
  out[1] = 0x01;
  out[2] = 0x1A;  // flags as the app sends them: it advertises connectable (ha-ble-adv sends 0x19)
  out[3] = (uint8_t)(1 + kMfgLen);
  out[4] = 0xFF;
  if (!encodeMfg(p, out + 5)) return 0;
  return need;
}

void nextSequence(Packet& p) {
  uint32_t n = (uint32_t)p.tx | ((uint32_t)p.restart << 8) | ((uint32_t)p.rem0 << 16) | ((uint32_t)(p.rem1 & 0x0F) << 24);
  n = (n + 1) & 0x0FFFFFFF;
  p.tx = (uint8_t)n;
  p.restart = (uint8_t)(n >> 8);
  p.rem0 = (uint8_t)(n >> 16);
  p.rem1 = (uint8_t)((p.rem1 & 0xF0) | ((n >> 24) & 0x0F));
}

}  // namespace agarce
