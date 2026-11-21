// BLE advertisement scanner: every advertisement heard is queued from NimBLE's task and printed from
// loop(), one line each, so the PC can capture the fan's commands as the Smart Light app sends them.
#pragma once
#include <stdint.h>

namespace scan {

struct Stats {
  uint32_t heard;    // advertisements received
  uint32_t agarce;   // of those, Smart Light (agarce) packets: manufacturer data F9 09 83|84
  uint32_t dropped;  // lost because the print queue was full
};

void begin();          // start NimBLE and a continuous passive scan, duplicates included
void setOnlyAgarce(bool only);  // print only Smart Light packets (the scan itself still counts everything)
void drain();          // print queued advertisements; call from loop()
Stats stats();

}  // namespace scan
