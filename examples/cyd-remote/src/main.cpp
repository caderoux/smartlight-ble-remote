// A touchscreen remote for a Smart Light (Argrace, "agarce") BLE fan-light, on an ESP32 "Cheap Yellow Display". The
// screen runs the fan; the serial console is for pairing, diagnostics and tests.
// Serial console (921600 baud), one command per line:
//   TAP <x> <y> (a tap at pixels on the screen showing, for unattended tests) | CALIBRATE (touch setup again) |
//   SKIP (cancel it)
//   HEAP | STATS | STATE (the fan state last sent) | ONLY AG (print only Smart Light adverts) | ALL (every advert)
//   SEND LIGHT ON|OFF | SEND BRIGHT <1-100> | SEND CT <0-100> (0 warm, 100 cold) | SEND FAN <1-6> | SEND FAN OFF
//   SEND FAN FWD|REV | SEND ALL OFF
//   NEWKEY (make the board's own key) | LEARN (adopt the ID of the next app command) | KEY (masked) | FORGET
//   PAIR <1-60> (PAIR 1: one pair command; longer: a fresh one every 2 s for that many seconds) | STOP
//   Protocol tools: PREFIX 83|84 | RAW <ms 20-2000> <hex> | DURATION <200-5000> (default 3000 ms, as the app) |
//   SET REM1 <hex> | SET RESTART <0-255> | SET INDEX <1-255> | GROUP <attr> <p0> <p1> <p2> <last> [type] (hex)
// Output lines: ADV ... (an AG advert's scrambled body is masked) · FAN ... · SENT ... · TAP <screen> ... ·
// SCREEN ... · TOUCH ... · CALIBRATE ... · PAIRING ... · PAIR done ... · NEWKEY ... · LEARNED ... · KEY ... ·
// STATE ... · HEAP ... · STATS ... · OK ... · ERR ...
#include <Arduino.h>

#include "fanlink.h"
#include "remote.h"
#include "scan.h"
#include "touchpad.h"
#include "ui.h"

namespace {

String inLine;
ui::Line line;                   // the status line: the last command's words and what became of it
remote::Screen screen = remote::Screen::Main;
bool cancelCalibration = false;  // SKIP: cancel the touch setup that's running or about to run
bool wantCalibration = false;    // CALIBRATE from the console (run from loop(), not inside the console handler)
bool screenDirty = true;         // redraw whichever screen is due from scratch (after the touch setup)
uint32_t lastTapMs = 0;          // the last tap that did something (sent, saved or changed screen)
uint32_t lastTouchMs = 0;        // the last tap at all (Settings goes back to Main after remote::kSettingsTimeoutMs)
// The screen acts on at most one tap per 400 ms: a quick double tap mustn't replace a command before the fan has
// caught it (each send also writes the key and the state to NVS), and the tap after a screen change mustn't land on
// the new screen's button under the finger (Settings on Main is where Direction is). The console keeps its own 1 s
// gap in fanlink.
constexpr uint32_t kTapGapMs = 400;

void printHeap(const char* when) {
  Serial.printf("HEAP %s free=%lu min=%lu\n", when, (unsigned long)ESP.getFreeHeap(),
                (unsigned long)ESP.getMinFreeHeap());
}

const char* screenName(remote::Screen s) { return s == remote::Screen::Main ? "main" : "settings"; }

void setLine(ui::Line::Kind kind, const char* words) {
  line.kind = kind;
  strncpy(line.label, words, sizeof(line.label) - 1);
  line.label[sizeof(line.label) - 1] = '\0';
}

// A tap on the screen at (x, y): from the panel, or TAP from the console
void tapAt(int x, int y) {
  lastTouchMs = millis();
  const char* on = screenName(screen);
  remote::Control c = remote::hit(screen, x, y);
  remote::Tap t = remote::tap(c, fanlink::state());
  if (t.kind == remote::Tap::Kind::Nothing || (t.kind != remote::Tap::Kind::Navigate && !fanlink::status().haveKey)) {
    Serial.printf("TAP %s %d %d -> control %d: nothing to do\n", on, x, y, (int)c);
    return;
  }
  if (millis() - lastTapMs < kTapGapMs) {
    Serial.printf("TAP %s %d %d -> control %d: ignored, within %lu ms of the last\n", on, x, y, (int)c,
                  (unsigned long)kTapGapMs);
    return;
  }
  lastTapMs = millis();
  if (t.kind == remote::Tap::Kind::Navigate) {
    screen = t.to;
    Serial.printf("TAP %s %d %d -> screen %s\n", on, x, y, screenName(screen));
    return;
  }
  char words[32];
  remote::label(t.action, t.value, words, sizeof(words));
  if (t.kind == remote::Tap::Kind::Save) {
    fanlink::remember(t.saved);
    Serial.printf("TAP %s %d %d -> %s: saved for on\n", on, x, y, words);
    setLine(ui::Line::Kind::Saved, words);
    return;
  }
  bool sent = fanlink::send(t.action, t.value);
  Serial.printf("TAP %s %d %d -> %s%s\n", on, x, y, words, sent ? "" : " (not sent)");
  // shown on the screen too: a tap must never just do nothing
  setLine(sent ? ui::Line::Kind::Sent : ui::Line::Kind::NotSent, words);
}

void handle(const String& cmd);

void readConsole() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      inLine.trim();
      handle(inLine);
      inLine = "";
    } else if (inLine.length() < 100) {  // RAW 2000 + 31 bytes of hex is 71
      inLine += c;
    }
  }
}

// While the touch setup waits for a person: keep the console and the scanner going; false = cancel it
bool calibrationIdle() {
  readConsole();
  scan::drain();
  fanlink::tick();  // a pairing broadcast keeps cycling and ends on time
  return !cancelCalibration;
}

void runCalibration() {
  if (!cancelCalibration) touchpad::calibrate(calibrationIdle);
  cancelCalibration = false;
  wantCalibration = false;  // a CALIBRATE typed during this one doesn't start another
  screenDirty = true;
}

void handle(const String& cmd) {
  int x, y;
  if (cmd.startsWith("TAP ") && sscanf(cmd.c_str() + 4, "%d %d", &x, &y) == 2) {
    tapAt(x, y);
  } else if (cmd == "CALIBRATE") {
    wantCalibration = true;
    cancelCalibration = false;
  } else if (cmd == "SKIP") {
    cancelCalibration = true;  // also cancels one asked for but not started yet
    Serial.println("OK skip");
  } else if (cmd == "HEAP") {
    printHeap("now");
  } else if (cmd == "ONLY AG") {
    scan::setOnlyAgarce(true);
    Serial.println("OK only agarce");
  } else if (cmd == "ALL") {
    scan::setOnlyAgarce(false);
    Serial.println("OK all");
  } else if (cmd == "STATS") {
    scan::Stats s = scan::stats();
    Serial.printf("STATS heard=%lu agarce=%lu dropped=%lu\n", (unsigned long)s.heard, (unsigned long)s.agarce,
                  (unsigned long)s.dropped);
  } else if (cmd == "NEWKEY") {
    fanlink::newKey();
  } else if (cmd.startsWith("PAIR ")) {
    fanlink::pair((uint32_t)cmd.substring(5).toInt());
  } else if (cmd.startsWith("PREFIX ")) {
    fanlink::setPrefix((uint8_t)strtoul(cmd.c_str() + 7, nullptr, 16));
  } else if (cmd.startsWith("GROUP ")) {
    fanlink::sendGroup(cmd.c_str() + 6);
  } else if (cmd.startsWith("SET ")) {
    fanlink::setField(cmd.c_str() + 4);
  } else if (cmd.startsWith("DURATION ")) {
    fanlink::setDuration((uint32_t)cmd.substring(9).toInt());
  } else if (cmd.startsWith("RAW ")) {
    fanlink::raw(cmd.c_str() + 4);
  } else if (cmd == "STOP") {
    fanlink::stop();
  } else if (cmd == "LEARN") {
    fanlink::learn();
  } else if (cmd == "STATE") {
    fanlink::printState();
  } else if (cmd == "KEY") {
    fanlink::printKey();
  } else if (cmd == "FORGET") {
    fanlink::forget();
  } else if (cmd.startsWith("SEND ")) {
    if (fanlink::send(cmd.c_str() + 5)) setLine(ui::Line::Kind::Sent, cmd.c_str() + 5);  // the words, e.g. "FAN 3"
  } else if (cmd.length() > 0) {
    Serial.printf("ERR unknown command: %s\n", cmd.c_str());
  }
}

void refreshScreen() {
  static bool showingNotPaired = false;
  fanlink::Status st = fanlink::status();
  if (!st.haveKey) {
    if (!showingNotPaired || screenDirty) ui::notPaired();
    showingNotPaired = true;
    screenDirty = false;
    return;
  }
  if (showingNotPaired || screenDirty) ui::invalidate();
  showingNotPaired = false;
  screenDirty = false;
  ui::remote(screen, fanlink::state(), st.onAir, line);
}

}  // namespace

void setup() {
  Serial.begin(921600);
  // A newline first: the ROM bootloader's banner (115200 baud, junk at this speed) ends without one, and the boot line
  // would be glued onto it, so line-based readers miss it.
  Serial.printf("\nBOOT smartlight remote \"%s\"\n", REMOTE_TITLE);
  printHeap("boot");
  ui::begin();
  fanlink::begin();
  touchpad::begin();
  printHeap("after_screen");
  scan::begin();
  printHeap("after_ble");
  if (!touchpad::calibrated()) runCalibration();
  refreshScreen();
}

void loop() {
  static uint32_t lastScreen = 0;
  readConsole();
  scan::drain();
  fanlink::tick();
  if (wantCalibration) {
    wantCalibration = false;
    runCalibration();
  }
  int x, y;
  if (touchpad::poll(x, y)) tapAt(x, y);
  // The desk normally shows the simple screen. Never mid-press (a tap counts on release, so the tap meant for a
  // Settings button would land on Main's LIGHT or FAN), and a release just after the switch is ignored too.
  if (screen == remote::Screen::Settings && !touchpad::touching() &&
      millis() - lastTouchMs >= remote::kSettingsTimeoutMs) {
    screen = remote::Screen::Main;
    lastTapMs = millis();
    Serial.println("SCREEN main (settings timed out)");
  }
  if (millis() - lastScreen >= 100) {
    lastScreen = millis();
    refreshScreen();
  }
  delay(2);
}
