#include "describe.h"

#include <stdio.h>
#include <string.h>

#include "fanbits.h"

namespace describe {

void packet(const agarce::Packet& p, char* out, size_t cap) {
  using namespace fanbits;
  if (p.group) {
    snprintf(out, cap, "group attr %02x params %02x %02x %02x last %02x", p.cmd, p.arg0, p.arg1, p.arg2, p.groupByte);
    return;
  }
  switch (p.cmd) {
    case 0x00:
      snprintf(out, cap, "%s", p.arg0 == 1 ? "pair" : "UNPAIR");
      return;
    case kLight:
      snprintf(out, cap, "light %s (ct %u, br %u)", p.arg0 ? "on" : "off", p.arg1, p.arg2);
      return;
    case kLightCtBr:
      snprintf(out, cap, "light ct %u br %u", p.arg0, p.arg1);
      return;
    case kAll:
      snprintf(out, cap, "all %s", p.arg0 >= 2 ? "on" : "off");
      return;
    case kFan: {
      char changed[48] = "";
      if (p.arg2 & kChgSpeed) strcat(changed, " speed");
      if (p.arg2 & kChgDir) strcat(changed, " dir");
      if (p.arg2 & kChgPreset) strcat(changed, " preset");
      if (p.arg2 & kChgOn) strcat(changed, " on");
      if (p.arg2 & kChgOsc) strcat(changed, " osc");
      snprintf(out, cap, "fan %s speed %u %s%s%s (changed:%s)", (p.arg0 & kFanOn) ? "on" : "off",
               p.arg0 & kFanSpeedMask, (p.arg0 & kFanReverse) ? "reverse" : "forward",
               (p.arg0 & kFanBreeze) ? " breeze" : "", p.arg1 ? " osc" : "", changed);
      return;
    }
    default:
      snprintf(out, cap, "cmd 0x%02x args %u,%u,%u", p.cmd, p.arg0, p.arg1, p.arg2);
  }
}

}  // namespace describe
