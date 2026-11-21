#include "fanstate.h"

#include "fanbits.h"

namespace fanstate {

void observe(const agarce::Packet& p, State& s) {
  using namespace fanbits;
  if (p.group) {
    // The group switch (attr 7): p2 says which it targets (1 fan, 2 light); the last byte carries both states
    if (p.cmd == kGroupSwitch) {
      if (p.arg2 & kSwitchLight) s.lightOn = (p.groupByte & kGroupLightOn) != 0;
      if (p.arg2 & kSwitchFan) s.fanOn = (p.groupByte & kGroupFanOn) != 0;
    }
    return;
  }
  switch (p.cmd) {
    case kLight:
      s.lightOn = p.arg0 != 0;
      s.ct = p.arg1;
      s.br = p.arg2;
      return;
    case kLightCtBr:
      s.ct = p.arg0;
      s.br = p.arg1;
      return;
    case kAll:  // all off (arg0 <= 1) or all on (>= 2)
      s.lightOn = p.arg0 >= 2;
      s.fanOn = p.arg0 >= 2;
      return;
    case kFan: {
      s.fanOn = (p.arg0 & kFanOn) != 0;
      s.reverse = (p.arg0 & kFanReverse) != 0;
      s.breeze = (p.arg0 & kFanBreeze) != 0;
      uint8_t speed = p.arg0 & kFanSpeedMask;
      if (speed >= 1 && speed <= 6) s.speed = speed;
      return;
    }
    default:
      return;
  }
}

}  // namespace fanstate
