// A packet's command in words, for the serial log ("light on (ct 100, br 100)", "fan on speed 2 forward ..."). Pure.
#pragma once
#include <stddef.h>

#include "agarce.h"

namespace describe {

void packet(const agarce::Packet& p, char* out, size_t cap);

}  // namespace describe
