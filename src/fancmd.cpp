#include "fancmd.h"

#include <string.h>

#include "fanbits.h"

namespace fancmd {
namespace {

using namespace fanbits;

uint8_t fanArg0(bool on, int speed, bool reverse, const fanstate::State& s) {
  return (uint8_t)((on ? kFanOn : 0) | (reverse ? kFanReverse : 0) | (s.breeze ? kFanBreeze : 0) |
                   (speed & kFanSpeedMask));
}

void fan(agarce::Packet& p, uint8_t arg0, uint8_t changed) {
  p.cmd = kFan;
  p.arg0 = arg0;
  p.arg1 = 0;
  p.arg2 = changed;
}

// "<prefix><digits>": 1-3 decimal digits only (no sign, spaces or leading zeros), the value in [lo, hi]
bool number(const char* text, const char* prefix, int lo, int hi, int& value) {
  size_t n = strlen(prefix);
  if (strncmp(text, prefix, n) != 0) return false;
  const char* d = text + n;
  size_t len = strlen(d);
  if (len < 1 || len > 3 || (len > 1 && d[0] == '0')) return false;
  int v = 0;
  for (size_t i = 0; i < len; i++) {
    if (d[i] < '0' || d[i] > '9') return false;
    v = v * 10 + (d[i] - '0');
  }
  if (v < lo || v > hi) return false;
  value = v;
  return true;
}

}  // namespace

bool build(Action action, int value, const fanstate::State& s, agarce::Packet& p) {
  switch (action) {
    case Action::LightOn:
    case Action::LightOff:
      p.cmd = kLight;
      p.arg0 = action == Action::LightOn ? 1 : 0;
      p.arg1 = s.ct;
      p.arg2 = s.br;
      return true;
    case Action::Brightness:
      if (value < 1 || value > 100) return false;
      p.cmd = kLightCtBr;
      p.arg0 = s.ct;
      p.arg1 = (uint8_t)value;
      p.arg2 = 0;
      return true;
    case Action::ColourTemp:
      if (value < 0 || value > 100) return false;
      p.cmd = kLightCtBr;
      p.arg0 = (uint8_t)value;
      p.arg1 = s.br;
      p.arg2 = 0;
      return true;
    case Action::FanSpeed:
      if (value < 1 || value > 6) return false;
      fan(p, fanArg0(true, value, s.reverse, s), kChgSpeed | kChgOn);
      return true;
    case Action::FanOff:
      // As the app's "fan off" vector: speed and direction carried, only "on" marked as changed
      fan(p, fanArg0(false, s.speed, s.reverse, s), kChgOn);
      return true;
    case Action::FanForward:
    case Action::FanReverse:
      // As the app's "fan dir reverse" vector: the whole state, only "direction" marked as changed
      fan(p, fanArg0(s.fanOn, s.speed, action == Action::FanReverse, s), kChgDir);
      return true;
    case Action::AllOff:
      // As ha-ble-adv's "all off" vector: args [1, 0, 100] (arg0 <= 1 means off)
      p.cmd = kAll;
      p.arg0 = 1;
      p.arg1 = 0;
      p.arg2 = 100;
      return true;
  }
  return false;
}

void buildPair(agarce::Packet& p) {
  p.cmd = 0x00;
  p.arg0 = 1;
  p.arg1 = 0;
  p.arg2 = 0;
}

bool parse(const char* text, Action& action, int& value) {
  value = 0;
  static const struct {
    const char* word;
    Action action;
  } kPlain[] = {{"LIGHT ON", Action::LightOn},     {"LIGHT OFF", Action::LightOff}, {"ALL OFF", Action::AllOff},
                {"FAN OFF", Action::FanOff},       {"FAN FWD", Action::FanForward}, {"FAN REV", Action::FanReverse}};
  for (const auto& w : kPlain) {
    if (strcmp(text, w.word) == 0) {
      action = w.action;
      return true;
    }
  }
  if (number(text, "BRIGHT ", 1, 100, value)) {
    action = Action::Brightness;
    return true;
  }
  if (number(text, "CT ", 0, 100, value)) {
    action = Action::ColourTemp;
    return true;
  }
  if (number(text, "FAN ", 1, 6, value)) {
    action = Action::FanSpeed;
    return true;
  }
  return false;
}

}  // namespace fancmd
