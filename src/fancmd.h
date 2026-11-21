// The remote's actions as fan commands: which packet each one is, built from the current state, and the console's
// words for them. Single-device commands (fanbits.h) to the key's group and device. Pure logic, tested on the PC.
#pragma once
#include <stdint.h>

#include "agarce.h"
#include "fanstate.h"

namespace fancmd {

enum class Action {
  LightOn,
  LightOff,
  Brightness,  // value 1-100
  ColourTemp,  // value 0 (warm) .. 100 (cold)
  FanSpeed,    // value 1-6 (turns the fan on)
  FanOff,
  FanForward,
  FanReverse,
  AllOff,
};

// Fill in a packet's command and arguments (the caller supplies identity, counters and seed). The state supplies
// everything the action doesn't change. Returns false for a value out of range.
bool build(Action action, int value, const fanstate::State& state, agarce::Packet& p);

// The pair command (cmd 0x00, arg0 1). There is deliberately no unpair builder.
void buildPair(agarce::Packet& p);

// Console words: "LIGHT ON", "LIGHT OFF", "BRIGHT <1-100>", "CT <0-100>", "FAN <1-6>", "FAN OFF", "FAN FWD",
// "FAN REV", "ALL OFF". Returns false if not one of them or the value is out of range.
bool parse(const char* text, Action& action, int& value);

}  // namespace fancmd
