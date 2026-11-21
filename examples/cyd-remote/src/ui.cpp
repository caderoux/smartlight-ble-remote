#include "ui.h"

#include <TFT_eSPI.h>
#include <string.h>

namespace ui {
namespace {

using remote::Control;
using remote::Rect;

TFT_eSPI tft;

// The colours (lighter, outlined idle buttons and white text, readable under glare), as RGB565
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
constexpr uint16_t kBg = rgb(11, 13, 16);
constexpr uint16_t kPanel = rgb(21, 24, 28);
constexpr uint16_t kIdle = rgb(59, 66, 75);
constexpr uint16_t kIdleEdge = rgb(163, 172, 182);
constexpr uint16_t kText = rgb(255, 255, 255);
constexpr uint16_t kMuted = rgb(201, 207, 214);
constexpr uint16_t kNote = rgb(221, 226, 231);
constexpr uint16_t kAmber = rgb(245, 181, 71);
constexpr uint16_t kAmberInk = rgb(26, 19, 0);
constexpr uint16_t kBlue = rgb(98, 176, 245);
constexpr uint16_t kBlueInk = rgb(4, 18, 31);
constexpr uint16_t kCool = rgb(159, 208, 255);
constexpr uint16_t kLightEdge = rgb(138, 106, 46);
constexpr uint16_t kFanEdge = rgb(47, 100, 148);

constexpr int kSmall = 2, kBig = 4;  // TFT_eSPI built-in fonts: 16 px and 26 px

// What's on the screen now, so remote() redraws only what changed
struct Drawn {
  bool valid = false;
  remote::Screen screen = remote::Screen::Main;
  bool lightOn = false, fanOn = false, reverse = false;
  uint8_t br = 255, ct = 255, speed = 0;
  bool sending = false;
  Line line;
};
Drawn drawn;

void text(const char* s, int x, int y, int font, uint16_t fg, uint16_t bg, uint8_t datum) {
  tft.setTextDatum(datum);
  tft.setTextColor(fg, bg);
  tft.drawString(s, x, y, font);
}

// A filled, outlined rounded box
void box(const Rect& r, int radius, uint16_t fill, uint16_t edge, int edgeWidth) {
  tft.fillRoundRect(r.x, r.y, r.w, r.h, radius, edge);
  tft.fillRoundRect(r.x + edgeWidth, r.y + edgeWidth, r.w - 2 * edgeWidth, r.h - 2 * edgeWidth,
                    radius > edgeWidth ? radius - edgeWidth : 1, fill);
}

void button(Control c, const char* s, uint16_t fill, uint16_t edge, uint16_t fg, int font) {
  Rect r = remote::rect(c);
  box(r, 8, fill, edge, 2);
  text(s, r.x + r.w / 2, r.y + r.h / 2, font, fg, fill, MC_DATUM);
}

void idleButton(Control c, const char* s, int font) { button(c, s, kIdle, kIdleEdge, kText, font); }

// --- Main ---

// LIGHT / FAN: a big on/off button with the settings the next "on" uses underneath
void bigButton(Control c, const char* name, bool on, uint16_t onColour, uint16_t onInk, const char* detail) {
  Rect r = remote::rect(c);
  uint16_t fill = on ? onColour : kIdle, fg = on ? onInk : kText;
  box(r, 12, fill, on ? onColour : kIdleEdge, 3);
  int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
  text(name, cx, cy - 34, kSmall, fg, fill, MC_DATUM);
  text(on ? "ON" : "OFF", cx, cy, kBig, fg, fill, MC_DATUM);
  text(detail, cx, cy + 34, kSmall, fg, fill, MC_DATUM);
}

void lightButton(const fanstate::State& s) {
  char detail[32];
  snprintf(detail, sizeof(detail), "%u%%  %s %u%%", s.br, s.ct <= 50 ? "warm" : "cool", s.ct);
  bigButton(Control::LightToggle, "LIGHT", s.lightOn, kAmber, kAmberInk, detail);
}

void fanButton(const fanstate::State& s) {
  char detail[32];
  snprintf(detail, sizeof(detail), "Speed %u  %s", s.speed, s.reverse ? "Reverse" : "Forward");
  bigButton(Control::FanToggle, "FAN", s.fanOn, kBlue, kBlueInk, detail);
}

void drawMainStatic() {
  tft.fillScreen(kBg);
  text(REMOTE_TITLE, 8, 13, kSmall, kText, kBg, ML_DATUM);
  Rect r = remote::rect(Control::OpenSettings);
  box(r, 10, kIdle, kIdleEdge, 2);
  text("Settings", r.x + r.w / 2 - 6, r.y + r.h / 2, kSmall, kText, kIdle, MC_DATUM);
  int x = r.x + r.w / 2 + 34, y = r.y + r.h / 2;  // a ">" chevron after the word
  for (int t = 0; t < 3; t++) {
    tft.drawLine(x + t, y - 5, x + 5 + t, y, kText);
    tft.drawLine(x + 5 + t, y, x + t, y + 5, kText);
  }
}

// --- Settings ---

void value(Control down, int v) {
  Rect r = remote::rect(down);
  // The whole gap between "-" and "+": font 4's "100%" is 63 px of the 64
  int x = r.x + r.w, w = remote::rect(Control::BrightUp).x - x;
  char s[8];
  snprintf(s, sizeof(s), "%d%%", v);
  tft.fillRect(x, r.y, w, r.h, kPanel);
  text(s, x + w / 2, r.y + r.h / 2, kBig, kText, kPanel, MC_DATUM);
}

void panelNote(const Rect& panel, bool on) {
  int y = panel.y + 10;
  tft.fillRect(panel.x + 120, y - 8, panel.w - 128, 16, kPanel);
  text(on ? "on: changes now" : "off: saved for on", panel.x + panel.w - 8, y, kSmall, kNote, kPanel, MR_DATUM);
}

void speeds(const fanstate::State& s) {
  static const char* const kDigits[] = {"1", "2", "3", "4", "5", "6"};
  for (int n = 1; n <= 6; n++) {
    Control c = (Control)((int)Control::Speed1 + n - 1);
    bool lit = remote::speedLit(c, s), marked = remote::speedMarked(c, s);
    button(c, kDigits[n - 1], lit ? kBlue : kIdle, marked ? kBlue : kIdleEdge, lit ? kBlueInk : kText, kBig);
  }
}

void drawSettingsStatic() {
  tft.fillScreen(kBg);
  Rect back = remote::rect(Control::Back);
  box(back, 8, kIdle, kIdleEdge, 2);
  text("Back", back.x + back.w / 2 + 6, back.y + back.h / 2, kSmall, kText, kIdle, MC_DATUM);
  int x = back.x + 14, y = back.y + back.h / 2;  // a "<" chevron before the word
  for (int t = 0; t < 3; t++) {
    tft.drawLine(x + 5 + t, y - 5, x + t, y, kText);
    tft.drawLine(x + t, y, x + 5 + t, y + 5, kText);
  }
  Rect lp = remote::lightPanel(), fp = remote::fanPanel();
  box(lp, 10, kPanel, kLightEdge, 2);
  box(fp, 10, kPanel, kFanEdge, 2);
  text("LIGHT", lp.x + 8, lp.y + 10, kSmall, kAmber, kPanel, ML_DATUM);
  text("FAN SPEED", fp.x + 8, fp.y + 10, kSmall, kBlue, kPanel, ML_DATUM);
  Rect bd = remote::rect(Control::BrightDown), cd = remote::rect(Control::ColourDown);
  text("Brightness", 12, bd.y + bd.h / 2, kSmall, kText, kPanel, ML_DATUM);
  int cy = cd.y + cd.h / 2;
  int cx = 12 + tft.textWidth("Colour ", kSmall);
  text("Colour", 12, cy, kSmall, kText, kPanel, ML_DATUM);
  text("warm", cx, cy, kSmall, kAmber, kPanel, ML_DATUM);
  cx += tft.textWidth("warm ", kSmall);
  text("-", cx, cy, kSmall, kMuted, kPanel, ML_DATUM);
  cx += tft.textWidth("- ", kSmall);
  text("cool", cx, cy, kSmall, kCool, kPanel, ML_DATUM);
  idleButton(Control::BrightDown, "-", kBig);
  idleButton(Control::BrightUp, "+", kBig);
  idleButton(Control::ColourDown, "-", kBig);
  idleButton(Control::ColourUp, "+", kBig);
}

// --- Both ---

void statusLine(remote::Screen screen, bool sending, const Line& line) {
  char s[48];
  bool alert = false;
  if (line.kind == Line::Kind::None || line.label[0] == '\0') {
    snprintf(s, sizeof(s), "Ready");
  } else if (line.kind == Line::Kind::Saved) {
    snprintf(s, sizeof(s), "Saved for on: %s", line.label);
  } else if (line.kind == Line::Kind::NotSent) {
    snprintf(s, sizeof(s), "Not sent: %s", line.label);
    alert = true;
  } else if (sending) {
    snprintf(s, sizeof(s), "Sending: %s...", line.label);
    alert = true;
  } else {
    snprintf(s, sizeof(s), "Sent: %s", line.label);
  }
  // Main: right of the title. Settings: right of Back.
  bool main = screen == remote::Screen::Main;
  int left = main ? 8 + tft.textWidth(REMOTE_TITLE, kSmall) + 8 : 84, top = 4, h = main ? 18 : 34;
  tft.fillRect(left, top, 316 - left, h, kBg);
  text(s, 314, top + h / 2, kSmall, alert ? kAmber : kMuted, kBg, MR_DATUM);
}

bool sameLine(const Line& a, const Line& b) { return a.kind == b.kind && strncmp(a.label, b.label, 31) == 0; }

}  // namespace

void begin() {
  tft.init();
  tft.setRotation(1);  // landscape, 320x240
  tft.fillScreen(kBg);
}

void invalidate() { drawn.valid = false; }

void remote(remote::Screen screen, const fanstate::State& s, bool sending, const Line& line) {
  bool all = !drawn.valid || screen != drawn.screen;
  if (screen == remote::Screen::Main) {
    if (all) drawMainStatic();
    if (all || s.lightOn != drawn.lightOn || s.br != drawn.br || s.ct != drawn.ct) lightButton(s);
    if (all || s.fanOn != drawn.fanOn || s.speed != drawn.speed || s.reverse != drawn.reverse) fanButton(s);
  } else {
    if (all) drawSettingsStatic();
    if (all || s.lightOn != drawn.lightOn) panelNote(remote::lightPanel(), s.lightOn);
    if (all || s.fanOn != drawn.fanOn) panelNote(remote::fanPanel(), s.fanOn);
    if (all || s.br != drawn.br) value(Control::BrightDown, s.br);
    if (all || s.ct != drawn.ct) value(Control::ColourDown, s.ct);
    if (all || s.fanOn != drawn.fanOn || s.speed != drawn.speed) speeds(s);
    if (all || s.reverse != drawn.reverse) idleButton(Control::Direction, s.reverse ? "<- Reverse" : "Forward ->", kSmall);
  }
  if (all || sending != drawn.sending || !sameLine(line, drawn.line)) statusLine(screen, sending, line);
  drawn.valid = true;
  drawn.screen = screen;
  drawn.lightOn = s.lightOn;
  drawn.fanOn = s.fanOn;
  drawn.reverse = s.reverse;
  drawn.br = s.br;
  drawn.ct = s.ct;
  drawn.speed = s.speed;
  drawn.sending = sending;
  drawn.line = line;
}

void notPaired() {
  drawn.valid = false;
  tft.fillScreen(kBg);
  text("Not paired", 160, 90, kBig, kAmber, kBg, MC_DATUM);
  text("This board has no key for the fan.", 160, 130, kSmall, kText, kBg, MC_DATUM);
  text("To pair it: see docs/pairing.md", 160, 154, kSmall, kMuted, kBg, MC_DATUM);
}

void calibrationTarget(int x, int y, int index, const char* message) {
  drawn.valid = false;
  tft.fillScreen(kBg);
  char step[24];
  snprintf(step, sizeof(step), "Touch setup %d of 5", index + 1);
  text(step, 160, 100, kSmall, kMuted, kBg, MC_DATUM);
  text(message, 160, 124, kSmall, kText, kBg, MC_DATUM);
  tft.drawLine(x - 10, y, x + 10, y, kAmber);
  tft.drawLine(x, y - 10, x, y + 10, kAmber);
  tft.drawCircle(x, y, 6, kAmber);
}

}  // namespace ui
