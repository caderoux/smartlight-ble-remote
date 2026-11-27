// Where the remote's controls are drawn, for one screen size: a value passed to remote::rect(), hit(), lightPanel()
// and fanPanel(), so one model serves several boards. Two are provided:
//   kLayout320x240: the 2.8" Cheap Yellow Display, landscape (the default everywhere).
//   kLayout480x320: a 4" 480x320 landscape panel with an 88 px rail of tabs down the left, which the remote leaves
//                   alone: its controls sit in x 100-468.
// A board with another screen can declare its own Layout.
#pragma once
#include <stdint.h>

namespace remote {

struct Rect {
  int16_t x, y, w, h;
  bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

struct Layout {
  int16_t width, height;  // the whole screen
  // Main: the LIGHT and FAN buttons side by side, then Settings across the bottom
  Rect lightToggle, fanToggle, openSettings;
  // Settings: Back (with the status beside it), the LIGHT panel (brightness, colour), the FAN panel (speeds,
  // direction)
  Rect back, lightPanel, fanPanel;
  Rect brightDown, brightUp, colourDown, colourUp;
  // Speeds 1-6 in a grid of `speedColumns` columns (6 = one row; 0 or less is taken as 6), left to right then top to
  // bottom
  int16_t speedX, speedY, speedW, speedH, speedGap, speedColumns;
  Rect direction;
  // Where each screen's status line goes ("Sent: Light on"): Main's above the buttons (beside the title, if the board
  // draws one), Settings' beside Back
  Rect mainStatus, settingsStatus;
};

// 4 px margins. Main: a header (title + status, y 4-22) above the buttons.
constexpr Layout kLayout320x240{
    320, 240,
    {4, 26, 153, 166}, {163, 26, 153, 166}, {4, 196, 312, 40},
    {4, 4, 76, 34}, {4, 42, 312, 98}, {4, 144, 312, 92},
    {140, 60, 52, 36}, {256, 60, 52, 36}, {140, 100, 52, 36}, {256, 100, 52, 36},
    12, 162, 46, 36, 4, 6,
    {12, 202, 296, 32},
    {4, 4, 312, 18}, {84, 4, 232, 34},
};

// Landscape, with the rail on the left: the area right of it, padded 10 px at the top and 12 px elsewhere.
// Main: a status line (y 10-32) above the buttons. Settings: the two panels side by side, the speeds in a 3x2 grid.
constexpr Layout kLayout480x320{
    480, 320,
    {100, 42, 178, 204}, {290, 42, 178, 204}, {100, 256, 368, 52},
    {100, 10, 84, 44}, {100, 64, 179, 244}, {289, 64, 179, 244},
    {108, 120, 44, 44}, {227, 120, 44, 44}, {108, 195, 44, 44}, {227, 195, 44, 44},
    297, 97, 50, 44, 6, 3,
    {297, 256, 163, 44},
    {100, 10, 368, 22}, {194, 10, 274, 44},
};

}  // namespace remote
