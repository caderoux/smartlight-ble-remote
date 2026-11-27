#include "remote.h"

#include <stdio.h>

namespace remote {
namespace {

int clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// One step from `v` in direction `dir` (+1/-1), landing on the 10% grid (a value of 37 steps to 40 or 30)
int stepped(int v, int dir, int lo, int hi) {
  int down = (v / kStep) * kStep;
  int next = dir > 0 ? down + kStep : (v % kStep ? down : down - kStep);
  return clamp(next, lo, hi);
}

// How far (px, the larger of the two axes) a point is outside a rect; 0 inside
int distance(const Rect& r, int x, int y) {
  int dx = x < r.x ? r.x - x : (x >= r.x + r.w ? x - (r.x + r.w - 1) : 0);
  int dy = y < r.y ? r.y - y : (y >= r.y + r.h ? y - (r.y + r.h - 1) : 0);
  return dx > dy ? dx : dy;
}

Tap send(fancmd::Action action, int value) {
  Tap t;
  t.kind = Tap::Kind::Send;
  t.action = action;
  t.value = value;
  return t;
}

Tap save(fancmd::Action action, int value, const fanstate::State& saved) {
  Tap t = send(action, value);
  t.kind = Tap::Kind::Save;
  t.saved = saved;
  return t;
}

Tap go(Screen to) {
  Tap t;
  t.kind = Tap::Kind::Navigate;
  t.to = to;
  return t;
}

}  // namespace

int speedOf(Control c) {
  int i = (int)c - (int)Control::Speed1;
  return i >= 0 && i < 6 ? i + 1 : 0;
}

Screen screenOf(Control c) {
  switch (c) {
    case Control::None:
    case Control::LightToggle:
    case Control::FanToggle:
    case Control::OpenSettings:
      return Screen::Main;
    default:
      return Screen::Settings;
  }
}

Rect lightPanel(const Layout& l) { return l.lightPanel; }
Rect fanPanel(const Layout& l) { return l.fanPanel; }

Rect rect(Control c, const Layout& l) {
  switch (c) {
    case Control::LightToggle: return l.lightToggle;
    case Control::FanToggle: return l.fanToggle;
    case Control::OpenSettings: return l.openSettings;
    case Control::Back: return l.back;
    case Control::BrightDown: return l.brightDown;
    case Control::BrightUp: return l.brightUp;
    case Control::ColourDown: return l.colourDown;
    case Control::ColourUp: return l.colourUp;
    case Control::Direction: return l.direction;
    case Control::None: return Rect{0, 0, 0, 0};
    default: {
      int cols = l.speedColumns > 0 ? l.speedColumns : 6;  // a Layout that left it unset: one row
      int i = speedOf(c) - 1, col = i % cols, row = i / cols;
      return Rect{(int16_t)(l.speedX + col * (l.speedW + l.speedGap)), (int16_t)(l.speedY + row * (l.speedH + l.speedGap)),
                  l.speedW, l.speedH};
    }
  }
}

Control hit(Screen screen, int x, int y, const Layout& l) {
  Control best = Control::None;
  int bestDistance = kSlop + 1;
  for (int i = 1; i < kControlCount; i++) {
    Control c = (Control)i;
    if (screenOf(c) != screen) continue;
    int d = distance(rect(c, l), x, y);
    if (d < bestDistance) {  // a tap inside a button (0) always beats a near miss of another
      best = c;
      bestDistance = d;
    }
  }
  return best;
}

Tap tap(Control c, const fanstate::State& s) {
  fanstate::State next = s;
  switch (c) {
    case Control::None:
      return Tap{};
    case Control::OpenSettings:
      return go(Screen::Settings);
    case Control::Back:
      return go(Screen::Main);
    case Control::LightToggle:
      // On with the brightness and colour last set: the light command carries them
      return send(s.lightOn ? fancmd::Action::LightOff : fancmd::Action::LightOn, 0);
    case Control::FanToggle:
      // On at the speed and direction last set
      return s.fanOn ? send(fancmd::Action::FanOff, 0) : send(fancmd::Action::FanSpeed, s.speed);
    case Control::BrightDown:
    case Control::BrightUp: {
      int v = stepped(s.br, c == Control::BrightUp ? 1 : -1, kBrightMin, kBrightMax);
      if (v == s.br) return Tap{};
      next.br = (uint8_t)v;
      return s.lightOn ? send(fancmd::Action::Brightness, v) : save(fancmd::Action::Brightness, v, next);
    }
    case Control::ColourDown:
    case Control::ColourUp: {
      int v = stepped(s.ct, c == Control::ColourUp ? 1 : -1, kColourMin, kColourMax);
      if (v == s.ct) return Tap{};
      next.ct = (uint8_t)v;
      return s.lightOn ? send(fancmd::Action::ColourTemp, v) : save(fancmd::Action::ColourTemp, v, next);
    }
    case Control::Direction: {
      fancmd::Action a = s.reverse ? fancmd::Action::FanForward : fancmd::Action::FanReverse;
      next.reverse = !s.reverse;
      return s.fanOn ? send(a, 0) : save(a, 0, next);
    }
    default: {
      int sp = speedOf(c);
      if (sp == 0 || sp == s.speed) return Tap{};  // already at that speed (or, while off, already the one set)
      next.speed = (uint8_t)sp;
      return s.fanOn ? send(fancmd::Action::FanSpeed, sp) : save(fancmd::Action::FanSpeed, sp, next);
    }
  }
}

void label(fancmd::Action action, int value, char* out, int cap) {
  switch (action) {
    case fancmd::Action::LightOn: snprintf(out, cap, "Light on"); return;
    case fancmd::Action::LightOff: snprintf(out, cap, "Light off"); return;
    case fancmd::Action::Brightness: snprintf(out, cap, "Brightness %d%%", value); return;
    case fancmd::Action::ColourTemp: snprintf(out, cap, "Colour %d%%", value); return;
    case fancmd::Action::FanSpeed: snprintf(out, cap, "Fan %d", value); return;
    case fancmd::Action::FanOff: snprintf(out, cap, "Fan off"); return;
    case fancmd::Action::FanForward: snprintf(out, cap, "Forward"); return;
    case fancmd::Action::FanReverse: snprintf(out, cap, "Reverse"); return;
    case fancmd::Action::AllOff: snprintf(out, cap, "All off"); return;
  }
  snprintf(out, cap, "?");
}

bool change(const fanstate::State& before, const fanstate::State& after, fancmd::Action& action, int& value) {
  using fancmd::Action;
  value = 0;
  if (before.lightOn && before.fanOn && !after.lightOn && !after.fanOn) {
    action = Action::AllOff;
  } else if (before.lightOn != after.lightOn) {
    action = after.lightOn ? Action::LightOn : Action::LightOff;
  } else if (before.fanOn != after.fanOn || (after.fanOn && before.speed != after.speed)) {
    action = after.fanOn ? Action::FanSpeed : Action::FanOff;
    value = after.fanOn ? after.speed : 0;
  } else if (before.reverse != after.reverse) {
    action = after.reverse ? Action::FanReverse : Action::FanForward;
  } else if (before.br != after.br) {
    action = Action::Brightness;
    value = after.br;
  } else if (before.ct != after.ct) {
    action = Action::ColourTemp;
    value = after.ct;
  } else {
    return false;
  }
  return true;
}

bool speedMarked(Control c, const fanstate::State& s) {
  int sp = speedOf(c);
  return sp != 0 && s.speed == sp;
}

bool speedLit(Control c, const fanstate::State& s) { return s.fanOn && speedMarked(c, s); }

}  // namespace remote
