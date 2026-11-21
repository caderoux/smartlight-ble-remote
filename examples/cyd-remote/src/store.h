// What the board keeps in NVS across reboots: the key (the identity it sends with, and its counters), the fan's state
// as last sent, and the touch calibration. One concern: persistence. Namespaces: "fan", "fanstate", "touch", "fp".
#pragma once
#include <stdint.h>

#include <string>

#include "fanstate.h"

namespace store {

struct Key {
  bool valid = false;
  bool own = false;  // made by NEWKEY (true) or learned from the app (false)
  uint32_t id = 0;
  uint8_t index = 0, prefix = 0x84, rem0 = 0x00, rem1 = 0x10;
  uint8_t tx = 0, restart = 0;  // the last counters used with this identity, by the board or the app
};

Key loadKey();
void saveKey(const Key& k);  // once per command (the counters), not per re-advertisement: NVS wears

// The fan's state as last sent: commands carry the whole state, so after a reboot a state reset to defaults would
// send stale brightness or direction. Written once per command.
fanstate::State loadState();
void saveState(const fanstate::State& s);

// A random number made on first use and kept ("fp"), for keying the ID fingerprints in logs. Never printed.
uint32_t fingerprintSalt();

// The touch calibration as pt::Calibration::toString() wrote it; empty when there's none.
std::string loadCalibration();
void saveCalibration(const std::string& text);

}  // namespace store
