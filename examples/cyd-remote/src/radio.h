// Putting advertisements on the air, as the vendor app does: connectable, a 20 ms interval, for a set time, one at a
// time (a new one replaces whatever is on the air). One concern: the BLE broadcaster.
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "agarce.h"

namespace radio {

bool putBytesOnAir(const uint8_t* adv, size_t len, uint32_t ms);  // false if advertising failed to start
bool putOnAir(const agarce::Packet& p, uint32_t ms);
void stop();
bool onAir();  // something is being advertised now

}  // namespace radio
