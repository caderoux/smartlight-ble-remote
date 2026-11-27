// Touch maths: taps from raw readings, and the calibration that maps them to pixels. Pure.
//
// Taps: a press counts once released, if it had at least `minReadings` readings (a graze or a light tap hovering at
// the threshold is ignored); a release is `releaseAfter` empty readings in a row, so a dropout mid-press doesn't
// split it. The tap's position is the median of its readings.
// Calibration: fitted from taps at known targets; detects whether the chip's axes are swapped against the screen's;
// worstError() catches a bounced or wrong tap during calibration.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pt {

struct Raw {
  int x, y;
};

class Taps {
 public:
  Taps(int minReadings = 3, int releaseAfter = 3) : minReadings(minReadings), releaseAfter(releaseAfter) {}
  // `pressed` false = nothing read this time. Returns true (and the tap in `out`) when a press ends as a tap.
  bool feed(bool pressed, Raw reading, Raw &out);
  bool down() const { return !xs.empty(); }
  static const int MAX_READINGS = 64;  // a press's median is taken over its first 64 readings (~1.3 s)

 private:
  int minReadings, releaseAfter;
  std::vector<int> xs, ys;
  int gap = 0;
};

int median(std::vector<int> values);  // the upper middle of an even count

struct Target {
  Raw raw;
  int sx, sy;  // where on the screen it was
};

class Calibration {
 public:
  bool swap = false;
  float ax = 1, bx = 0, ay = 1, by = 0;
  // The screen toScreen() clamps to. fit() sets it; toString() doesn't save it, so after parse() set it to the
  // board's screen, or a larger screen's taps are clamped to 320x240.
  int width = 320, height = 240;

  // false when the taps don't spread across the screen
  static bool fit(const std::vector<Target> &taps, int width, int height, Calibration &out);
  void toScreen(Raw raw, int &x, int &y) const;
  int worstError(const std::vector<Target> &taps) const;
  // Saved in the board's NVS as "swap,ax,bx,ay,by"; parse() accepts exactly that, nothing more or less.
  std::string toString() const;
  bool parse(const std::string &text);
};

}  // namespace pt
