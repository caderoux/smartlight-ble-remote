// The Smart Light app's BLE packet format ("agarce"), ported from NicoIIT/ha-ble-adv (MIT),
// custom_components/ble_adv/codecs/agarce.py at commit 484371f8, AgarceEncoder (the app's v3/v4 packets; not the
// handheld remote's "vr" variants). Pair and unpair packets decode; only PAIR can be built, never unpair.
//
// On the air: an advertisement whose manufacturer data (AD type 0xFF) is 20 bytes:
//   F9 09 | prefix (0x83 = v3, 0x84 = v4) | seed lo, seed hi | 14 scrambled bytes | outer checksum
// The 14 scrambled bytes, XORed with the seed pivot [lo, hi, hi, lo] and the matrix AA BB CC DD 5A A5 A5 5A, are:
//   tx | restart | rem0 rem1 | id (4, little-endian) | cmd (high nibble) + index low nibble | arg0 arg1 arg2 |
//   index high nibble | inner checksum (sum of the 13 before it)
// The outer checksum is the sum of the 17 bytes before it (prefix, seed and the scrambled bytes).
// Pure logic: no Arduino code, tested on the PC (pio test -e native).
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace agarce {

constexpr size_t kMfgLen = 20;  // F9 09 + 18

struct Packet {
  uint8_t prefix = 0x84;  // 0x83 (v3) or 0x84 (v4)
  uint16_t seed = 0;
  uint8_t tx = 0;         // the app's per-command counter
  uint8_t restart = 0;    // the app's restart counter
  uint8_t rem0 = 0x00, rem1 = 0x10;  // unknown; ha-ble-adv always sends 00 10. Copy what the app sends.
  uint32_t id = 0;        // the app's ID: the key to the fan
  uint8_t index = 0;      // the device index within the app
  uint8_t cmd = 0;        // command, high nibble only: 0x00 pair (arg0 1) / unpair (arg0 0), 0x10 light,
                          // 0x20 CT/brightness, 0x70 device, 0x80 fan
  uint8_t arg0 = 0, arg1 = 0, arg2 = 0;
  // A GROUP command (as the app sends it; see also NicoIIT's gist b3acaa75): the attribute (1-15) in the
  // whole command byte, arg0-2 its params, and groupByte = group | assist<<6 | on<<7 in the index byte. No device ID:
  // it reaches every device paired to the group. encodeMfg only; such packets decode as pair packets.
  bool group = false;
  uint8_t groupByte = 0;
};

// GroupPacket: a pair/unpair addressed to a group (index byte 0), which ha-ble-adv also excludes
enum class Decode { Ok, NotAgarce, BadChecksum, GroupPacket };

// Cheap check, safe to call from the BLE task: does this advertisement carry Smart Light manufacturer data
// (F9 09 then prefix 0x83/0x84, or a pair packet's 0x03/0x04)? It doesn't verify checksums; decodeAdv does.
bool isAgarceAdv(const uint8_t* adv, size_t len);
// Find the manufacturer data in a raw advertisement payload (AD structures) and decode it.
Decode decodeAdv(const uint8_t* adv, size_t len, Packet& out);
// Decode 20 bytes of manufacturer data (starting F9 09).
Decode decodeMfg(const uint8_t* mfg, size_t len, Packet& out);
// For a packet that doesn't decode: the wire prefix, both checksums (received/computed) and the unscrambled body,
// with the four ID bytes shown as "xx". Text like "p=03 outer=46/46 inner=c1/b1 d=02 00 00 10 xx xx xx xx 00 01 08 01 c1".
void diagnose(const uint8_t* adv, size_t len, char* out, size_t cap);
// Build the 20 bytes of manufacturer data for a packet. Returns false for unpair (cmd 0x00 with arg0 != 1).
bool encodeMfg(const Packet& p, uint8_t out[kMfgLen]);
// A full advertisement payload as the app sends it: flags (02 01 1A), then the manufacturer data. Returns its length.
size_t encodeAdv(const Packet& p, uint8_t* out, size_t cap);

// The packet's sequence number: tx, restart, rem0 and rem1's low nibble are one 28-bit little-endian counter (rem1's
// high nibble is the app's "order", not part of it). The receiver expects it to keep increasing.
uint32_t sequence(const Packet& p);
// Step the packet's sequence number by one, keeping rem1's high nibble.
void nextSequence(Packet& p);
// Whether a's sequence number comes after b's: true when it's 1 to 0x07FFFFFF ahead (under half the 28-bit range), so
// it holds across the wrap from 0x0FFFFFFF to 0 and is never true both ways. Equal is not newer. Two controllers
// sharing an ID use it to follow each other.
bool newer(const Packet& a, const Packet& b);

}  // namespace agarce
