// The board's screen: the remote's two screens (main: light and fan on/off; settings), drawn from the library's remote
// layout and the state the board last sent; the calibration targets; and "Not paired". Only what changed is redrawn.
#pragma once

#include "fanstate.h"
#include "remote.h"

// The main screen's title: a build setting (platformio.ini: '-DREMOTE_TITLE="Kitchen fan"'), so one firmware can name
// each room's remote.
#ifndef REMOTE_TITLE
#define REMOTE_TITLE "Fan"
#endif

namespace ui {

// The status line: the last command's words and what became of it. While it's on the air it reads "Sending: ...".
struct Line {
  enum class Kind : uint8_t {
    None,     // "Ready"
    Sent,     // "Sent: Fan 3"
    NotSent,  // "Not sent: Fan 3" (refused: a tap must never just do nothing)
    Saved,    // "Saved for on: Fan 4" (a setting changed while its device is off)
  };
  Kind kind = Kind::None;
  char label[32] = "";
};

void begin();
// The remote, on `screen`. `sending` = a command is on the air. Cheap to call every loop: draws only what changed
// since the last call (everything after a screen change or invalidate()).
void remote(remote::Screen screen, const fanstate::State& s, bool sending, const Line& line);
void notPaired();  // the board has no key: it can't control the fan
void calibrationTarget(int x, int y, int index, const char* message);
void invalidate();  // the next remote() redraws everything (after another screen was shown)

}  // namespace ui
