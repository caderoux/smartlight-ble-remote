#include "touchpad.h"

#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

#include <vector>

#include "pt_touch.h"
#include "store.h"
#include "ui.h"

namespace touchpad {
namespace {

// The ESP32-32E CYD's touch controller on its own SPI bus
constexpr int kClk = 25, kMiso = 39, kMosi = 32, kCs = 33, kIrq = 36;
constexpr int kTargets[5][2] = {{20, 20}, {300, 20}, {20, 220}, {300, 220}, {160, 120}};
constexpr int kMaxCalError = 20;  // px: a bounced or wrong calibration tap is caught
constexpr uint32_t kPollMs = 15;

SPIClass touchSpi(VSPI);
XPT2046_Touchscreen touch(kCs, kIrq);
pt::Calibration cal;
bool haveCal = false;
pt::Taps taps;
uint32_t lastPoll = 0;

bool read(pt::Raw& raw) {
  if (!touch.touched()) return false;
  TS_Point p = touch.getPoint();
  raw = pt::Raw{p.x, p.y};
  return true;
}

}  // namespace

void begin() {
  touchSpi.begin(kClk, kMiso, kMosi, kCs);
  touch.begin(touchSpi);
  touch.setRotation(0);  // raw chip coordinates: the calibration maps them (it detects swapped axes)
  std::string text = store::loadCalibration();
  haveCal = !text.empty() && cal.parse(text);
  Serial.printf("TOUCH %s\n", haveCal ? "calibration loaded" : "no calibration");
}

bool calibrated() { return haveCal; }

bool calibrate(bool (*idle)()) {
  const char* message = "Tap the cross firmly";
  for (;;) {
    std::vector<pt::Target> found;
    pt::Taps calTaps;
    for (int i = 0; i < 5; i++) {
      ui::calibrationTarget(kTargets[i][0], kTargets[i][1], i, message);
      Serial.printf("CALIBRATE tap target %d at %d,%d\n", i + 1, kTargets[i][0], kTargets[i][1]);
      pt::Raw raw;
      for (;;) {
        pt::Raw reading{0, 0};
        bool down = read(reading);
        if (calTaps.feed(down, reading, raw)) break;
        if (!idle()) {
          Serial.println("CALIBRATE cancelled; the old calibration is kept");
          return false;
        }
        delay(20);
      }
      found.push_back(pt::Target{raw, kTargets[i][0], kTargets[i][1]});
      delay(300);  // a pause between targets, so one press can't count for two
    }
    pt::Calibration fitted;
    int error = -1;
    if (pt::Calibration::fit(found, 320, 240, fitted)) error = fitted.worstError(found);
    if (error >= 0 && error <= kMaxCalError) {
      cal = fitted;
      haveCal = true;
      store::saveCalibration(cal.toString());
      Serial.printf("CALIBRATE saved (worst %d px)\n", error);
      return true;
    }
    Serial.printf("CALIBRATE worst error %d px; again\n", error);
    message = "Missed one: once more, firmly";
  }
}

bool poll(int& x, int& y) {
  if (!haveCal || millis() - lastPoll < kPollMs) return false;
  lastPoll = millis();
  pt::Raw reading{0, 0}, tap;
  bool down = read(reading);
  if (!taps.feed(down, reading, tap)) return false;
  cal.toScreen(tap, x, y);
  return true;
}

bool touching() { return taps.down(); }

}  // namespace touchpad
