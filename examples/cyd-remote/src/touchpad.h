// The touch panel (XPT2046, resistive): raw readings → taps → screen pixels, with the calibration that maps them
// (the library's pt_touch). The calibration is stored in NVS ("touch"); without one, the board asks for it.
#pragma once

namespace touchpad {

void begin();       // start the panel's SPI and load the stored calibration
bool calibrated();  // a usable calibration is loaded
// Shows five targets (ui::calibrationTarget) and fits a calibration from the taps; asks again until the taps agree
// within 20 px, then saves it to NVS. While it waits for a person it calls `idle()` (the console, the scanner) and
// gives up, keeping the old calibration, as soon as `idle()` returns false. Returns true if it saved a new one.
bool calibrate(bool (*idle)());
// Call every loop: true (with the screen pixel) when a press ends as a tap. Polls the panel at most every 15 ms.
bool poll(int& x, int& y);
// A finger is on the panel now (a press that hasn't ended as a tap yet)
bool touching();

}  // namespace touchpad
