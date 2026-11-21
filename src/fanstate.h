// What the board knows of the fan's state. The fan never reports it, so this is what was last sent (or heard from
// the app). Every command carries the whole state, as the app's do: a command changes only what it means to change.
// Pure logic, tested on the PC.
#pragma once
#include <stdint.h>

#include "agarce.h"

namespace fanstate {

struct State {
  bool lightOn = true;   // the fan's light comes on at power-up
  uint8_t br = 100;      // brightness 1-100
  uint8_t ct = 100;      // colour temperature 0 (warm) .. 100 (cold), as the app sends it
  bool fanOn = false;
  uint8_t speed = 1;     // the last speed set (1-6), kept while the fan is off
  bool reverse = false;
  bool breeze = false;
};

// Update the state from a packet the board sent or heard.
void observe(const agarce::Packet& p, State& s);

}  // namespace fanstate
