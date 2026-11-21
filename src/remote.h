// The touchscreen remote's model: where each control is on each screen, which control a tap hits, and what a tap
// does. Pure logic, tested on the PC; the firmware's ui.cpp draws it from the same layout. 320x240 landscape.
//   Main: LIGHT on/off and FAN on/off (each turns on with the last settings), and Settings.
//   Settings: Back; brightness and colour -/+; fan speeds 1-6 and direction. A setting changed while its device is
//   off is only saved, for the next "on" from the main screen; while it's on, it's sent.
#pragma once
#include <stdint.h>

#include "fancmd.h"
#include "fanstate.h"

namespace remote {

enum class Screen : uint8_t { Main, Settings };

enum class Control : uint8_t {
  None,
  // Main
  LightToggle,
  FanToggle,
  OpenSettings,
  // Settings
  Back,
  BrightDown,
  BrightUp,
  ColourDown,  // warmer
  ColourUp,    // cooler
  Speed1,
  Speed2,
  Speed3,
  Speed4,
  Speed5,
  Speed6,
  Direction,
};
constexpr int kControlCount = 16;  // including None

struct Rect {
  int16_t x, y, w, h;
  bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

constexpr int kWidth = 320, kHeight = 240;
constexpr int kStep = 10;  // brightness and colour move in steps of 10%
constexpr int kBrightMin = 10, kBrightMax = 100, kColourMin = 0, kColourMax = 100;
// A tap this many pixels outside a button still counts as that button (on a resistive panel, about 1 tap in 8
// lands a few pixels outside the button aimed at).
constexpr int kSlop = 4;
constexpr uint32_t kSettingsTimeoutMs = 30000;  // Settings goes back to Main after this long with no tap

Screen screenOf(Control c);  // which screen a control is on (Main for None)
// The control's drawn rect (an empty rect for None).
Rect rect(Control c);
// The settings screen's panels (for drawing): the light's and the fan's.
Rect lightPanel();
Rect fanPanel();

// The control under a tap on `screen`: the one drawn there, else the nearest within kSlop of the tap, else None.
Control hit(Screen screen, int x, int y);

// What a tap does.
struct Tap {
  enum class Kind : uint8_t {
    Nothing,   // no control, or it would change nothing (brightness already at 100 and "+")
    Send,      // send `action`/`value`
    Save,      // the device is off: keep `saved` as the state, send nothing (`action`/`value` name the change)
    Navigate,  // show `to`
  };
  Kind kind = Kind::Nothing;
  fancmd::Action action = fancmd::Action::LightOn;
  int value = 0;
  fanstate::State saved;
  Screen to = Screen::Main;
};
Tap tap(Control c, const fanstate::State& s);

// The status line's words for a command: "Light on", "Brightness 70%", "Colour 30%", "Fan 3", "Fan off", "Reverse".
void label(fancmd::Action action, int value, char* out, int cap);

// For drawing a speed button: `lit` = the fan is running at it; `marked` = it's the speed set (shown even while the
// fan is off, as the speed the next "on" uses).
bool speedLit(Control c, const fanstate::State& s);
bool speedMarked(Control c, const fanstate::State& s);
int speedOf(Control c);  // 1-6 for Speed1-Speed6, else 0

}  // namespace remote
