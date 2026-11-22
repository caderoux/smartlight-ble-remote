// The touchscreen remote's model (src/remote): both screens' layout, hit-testing and what each tap does.
#include <unity.h>

#include "remote.h"

using remote::Control;
using remote::Screen;
using Kind = remote::Tap::Kind;

namespace {

bool overlaps(const remote::Rect& a, const remote::Rect& b) {
  return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

bool inside(const remote::Rect& inner, const remote::Rect& outer) {
  return inner.x >= outer.x && inner.y >= outer.y && inner.x + inner.w <= outer.x + outer.w &&
         inner.y + inner.h <= outer.y + outer.h;
}

bool onLightPanel(Control c) {
  return c == Control::BrightDown || c == Control::BrightUp || c == Control::ColourDown || c == Control::ColourUp;
}

// Every control is on screen and big enough to tap; the settings controls sit inside their panel; no two controls
// on the same screen overlap.
void test_layout_is_sound() {
  int checked[2] = {0, 0};
  for (int i = 1; i < remote::kControlCount; i++) {
    Control c = (Control)i;
    remote::Rect r = remote::rect(c);
    TEST_ASSERT_TRUE_MESSAGE(r.w >= 32 && r.h >= 32, "target too small");  // + kSlop each side: >= 40 to a finger
    TEST_ASSERT_TRUE(r.x >= 0 && r.y >= 0 && r.x + r.w <= remote::kWidth && r.y + r.h <= remote::kHeight);
    if (onLightPanel(c)) TEST_ASSERT_TRUE(inside(r, remote::lightPanel()));
    if (remote::speedOf(c) || c == Control::Direction) TEST_ASSERT_TRUE(inside(r, remote::fanPanel()));
    for (int j = i + 1; j < remote::kControlCount; j++) {
      Control d = (Control)j;
      if (remote::screenOf(d) == remote::screenOf(c)) TEST_ASSERT_FALSE(overlaps(r, remote::rect(d)));
    }
    checked[(int)remote::screenOf(c)]++;
  }
  TEST_ASSERT_EQUAL(3, checked[0]);   // anti-vacuity: Main has 3 controls
  TEST_ASSERT_EQUAL(12, checked[1]);  // Settings has 12
  TEST_ASSERT_FALSE(overlaps(remote::lightPanel(), remote::fanPanel()));
  TEST_ASSERT_FALSE(overlaps(remote::rect(Control::Back), remote::lightPanel()));
}

// A tap at each control's centre hits it, on its own screen only; the header and the gaps hit nothing.
void test_hit() {
  for (int i = 1; i < remote::kControlCount; i++) {
    Control c = (Control)i;
    remote::Rect r = remote::rect(c);
    Screen own = remote::screenOf(c);
    Screen other = own == Screen::Main ? Screen::Settings : Screen::Main;
    TEST_ASSERT_TRUE(remote::hit(own, r.x + r.w / 2, r.y + r.h / 2) == c);
    Control there = remote::hit(other, r.x + r.w / 2, r.y + r.h / 2);  // never a control of the screen not shown
    TEST_ASSERT_TRUE(there == Control::None || remote::screenOf(there) == other);
  }
  TEST_ASSERT_TRUE(remote::hit(Screen::Main, 160, 10) == Control::None);      // the header
  TEST_ASSERT_TRUE(remote::hit(Screen::Settings, 200, 20) == Control::None);  // the status line
  TEST_ASSERT_TRUE(remote::hit(Screen::Settings, 60, 80) == Control::None);   // the "Brightness" words
  TEST_ASSERT_TRUE(remote::hit(Screen::Main, -1, -1) == Control::None);
  TEST_ASSERT_TRUE(remote::hit(Screen::Settings, 400, 300) == Control::None);
}

// Near misses (on a resistive panel, taps often land 1-6 px outside a button): within kSlop they count, beyond it they don't, and a tap
// inside a button always beats a near miss of its neighbour.
void test_near_misses() {
  remote::Rect up = remote::rect(Control::BrightUp);
  TEST_ASSERT_TRUE(remote::hit(Screen::Settings, up.x + 10, up.y - remote::kSlop) == Control::BrightUp);
  TEST_ASSERT_TRUE(remote::hit(Screen::Settings, up.x + 10, up.y - remote::kSlop - 1) == Control::None);
  remote::Rect light = remote::rect(Control::LightToggle);
  TEST_ASSERT_TRUE(remote::hit(Screen::Main, light.x + 20, light.y - 3) == Control::LightToggle);
  // Between speeds 1 and 2 (a 4 px gap): each side goes to the nearer one
  remote::Rect s1 = remote::rect(Control::Speed1), s2 = remote::rect(Control::Speed2);
  TEST_ASSERT_TRUE(remote::hit(Screen::Settings, s1.x + s1.w, s1.y + 10) == Control::Speed1);
  TEST_ASSERT_TRUE(remote::hit(Screen::Settings, s2.x - 1, s2.y + 10) == Control::Speed2);
  TEST_ASSERT_TRUE(remote::hit(Screen::Settings, s2.x, s2.y + 10) == Control::Speed2);
  // Inside Direction, right under speed 3's slop: Direction wins
  remote::Rect dir = remote::rect(Control::Direction);
  TEST_ASSERT_TRUE(remote::hit(Screen::Settings, 120, dir.y) == Control::Direction);
}

void test_main_screen() {
  fanstate::State s;
  s.lightOn = true;
  remote::Tap t = remote::tap(Control::LightToggle, s);
  TEST_ASSERT_TRUE(t.kind == Kind::Send && t.action == fancmd::Action::LightOff);
  s.lightOn = false;
  t = remote::tap(Control::LightToggle, s);
  TEST_ASSERT_TRUE(t.kind == Kind::Send && t.action == fancmd::Action::LightOn);
  // Fan on = the speed last set (its direction rides in the command, from the state)
  s.fanOn = false;
  s.speed = 4;
  t = remote::tap(Control::FanToggle, s);
  TEST_ASSERT_TRUE(t.kind == Kind::Send && t.action == fancmd::Action::FanSpeed);
  TEST_ASSERT_EQUAL(4, t.value);
  s.fanOn = true;
  t = remote::tap(Control::FanToggle, s);
  TEST_ASSERT_TRUE(t.kind == Kind::Send && t.action == fancmd::Action::FanOff);
  t = remote::tap(Control::OpenSettings, s);
  TEST_ASSERT_TRUE(t.kind == Kind::Navigate && t.to == Screen::Settings);
  t = remote::tap(Control::Back, s);
  TEST_ASSERT_TRUE(t.kind == Kind::Navigate && t.to == Screen::Main);
  TEST_ASSERT_TRUE(remote::tap(Control::None, s).kind == Kind::Nothing);
}

// Light on: brightness and colour are sent, on the 10% grid, clamped; at a limit nothing happens.
void test_light_settings_while_on() {
  fanstate::State s;
  s.lightOn = true;
  s.br = 70;
  remote::Tap t = remote::tap(Control::BrightDown, s);
  TEST_ASSERT_TRUE(t.kind == Kind::Send && t.action == fancmd::Action::Brightness);
  TEST_ASSERT_EQUAL(60, t.value);
  TEST_ASSERT_EQUAL(80, remote::tap(Control::BrightUp, s).value);
  s.br = 37;  // off-grid (set from the console): steps land on the grid
  TEST_ASSERT_EQUAL(40, remote::tap(Control::BrightUp, s).value);
  TEST_ASSERT_EQUAL(30, remote::tap(Control::BrightDown, s).value);
  s.br = 100;
  TEST_ASSERT_TRUE(remote::tap(Control::BrightUp, s).kind == Kind::Nothing);
  s.br = 10;
  TEST_ASSERT_TRUE(remote::tap(Control::BrightDown, s).kind == Kind::Nothing);
  s.br = 5;  // below the floor (only the console can set it): "-" clamps to 10, which is still a change
  TEST_ASSERT_EQUAL(10, remote::tap(Control::BrightDown, s).value);
  s.ct = 0;
  TEST_ASSERT_TRUE(remote::tap(Control::ColourDown, s).kind == Kind::Nothing);
  t = remote::tap(Control::ColourUp, s);
  TEST_ASSERT_TRUE(t.kind == Kind::Send && t.action == fancmd::Action::ColourTemp);
  TEST_ASSERT_EQUAL(10, t.value);
  s.ct = 100;
  TEST_ASSERT_TRUE(remote::tap(Control::ColourUp, s).kind == Kind::Nothing);
  TEST_ASSERT_EQUAL(90, remote::tap(Control::ColourDown, s).value);
}

// Light off: the change is saved for the next "on", nothing is sent, and nothing else in the state moves.
void test_light_settings_while_off() {
  fanstate::State s;
  s.lightOn = false;
  s.br = 70;
  s.ct = 50;
  s.fanOn = true;
  s.speed = 2;
  remote::Tap t = remote::tap(Control::BrightUp, s);
  TEST_ASSERT_TRUE(t.kind == Kind::Save && t.action == fancmd::Action::Brightness);
  TEST_ASSERT_EQUAL(80, t.value);
  TEST_ASSERT_EQUAL(80, t.saved.br);
  TEST_ASSERT_EQUAL(50, t.saved.ct);
  TEST_ASSERT_FALSE(t.saved.lightOn);
  TEST_ASSERT_TRUE(t.saved.fanOn);
  TEST_ASSERT_EQUAL(2, t.saved.speed);
  t = remote::tap(Control::ColourDown, s);
  TEST_ASSERT_TRUE(t.kind == Kind::Save);
  TEST_ASSERT_EQUAL(40, t.saved.ct);
  TEST_ASSERT_EQUAL(70, t.saved.br);
  s.br = 100;
  TEST_ASSERT_TRUE(remote::tap(Control::BrightUp, s).kind == Kind::Nothing);  // a limit is still a limit
}

void test_fan_settings() {
  fanstate::State s;
  s.fanOn = true;
  s.speed = 3;
  s.reverse = false;
  remote::Tap t = remote::tap(Control::Speed5, s);
  TEST_ASSERT_TRUE(t.kind == Kind::Send && t.action == fancmd::Action::FanSpeed);
  TEST_ASSERT_EQUAL(5, t.value);
  TEST_ASSERT_TRUE(remote::tap(Control::Speed3, s).kind == Kind::Nothing);  // already running at 3
  t = remote::tap(Control::Direction, s);
  TEST_ASSERT_TRUE(t.kind == Kind::Send && t.action == fancmd::Action::FanReverse);
  s.reverse = true;
  TEST_ASSERT_TRUE(remote::tap(Control::Direction, s).action == fancmd::Action::FanForward);
  // Off: saved, not sent; the fan stays off
  s.fanOn = false;
  t = remote::tap(Control::Speed6, s);
  TEST_ASSERT_TRUE(t.kind == Kind::Save && t.action == fancmd::Action::FanSpeed);
  TEST_ASSERT_EQUAL(6, t.saved.speed);
  TEST_ASSERT_FALSE(t.saved.fanOn);
  TEST_ASSERT_TRUE(t.saved.reverse);
  t = remote::tap(Control::Direction, s);
  TEST_ASSERT_TRUE(t.kind == Kind::Save && t.action == fancmd::Action::FanForward);
  TEST_ASSERT_FALSE(t.saved.reverse);
  TEST_ASSERT_EQUAL(3, t.saved.speed);
  TEST_ASSERT_TRUE(remote::tap(Control::Speed3, s).kind == Kind::Nothing);  // already the speed set
  // Drawing: the set speed is marked even while off, lit only while running
  TEST_ASSERT_TRUE(remote::speedMarked(Control::Speed3, s));
  TEST_ASSERT_FALSE(remote::speedLit(Control::Speed3, s));
  s.fanOn = true;
  TEST_ASSERT_TRUE(remote::speedLit(Control::Speed3, s));
  TEST_ASSERT_FALSE(remote::speedLit(Control::Speed4, s));
  TEST_ASSERT_FALSE(remote::speedMarked(Control::Direction, s));
}

void test_labels() {
  char text[32];
  remote::label(fancmd::Action::Brightness, 70, text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("Brightness 70%", text);
  remote::label(fancmd::Action::FanSpeed, 3, text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("Fan 3", text);
  remote::label(fancmd::Action::FanReverse, 0, text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("Reverse", text);
  remote::label(fancmd::Action::LightOff, 0, text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("Light off", text);
}

// change() then label(): the words a remote shows for a command another controller sent
const char* heard(const fanstate::State& before, const fanstate::State& after) {
  static char text[32];
  fancmd::Action action;
  int value;
  if (!remote::change(before, after, action, value)) return "(nothing)";
  remote::label(action, value, text, sizeof(text));
  return text;
}

void test_change_names_what_another_controller_did() {
  fanstate::State s;  // light on, br 100, ct 100, fan off at speed 1
  fanstate::State t = s;
  TEST_ASSERT_EQUAL_STRING("(nothing)", heard(s, t));
  t.lightOn = false;
  TEST_ASSERT_EQUAL_STRING("Light off", heard(s, t));
  TEST_ASSERT_EQUAL_STRING("Light on", heard(t, s));
  t = s;
  t.fanOn = true;
  t.speed = 3;
  TEST_ASSERT_EQUAL_STRING("Fan 3", heard(s, t));  // on at a speed
  TEST_ASSERT_EQUAL_STRING("Fan off", heard(t, s));
  fanstate::State u = t;
  u.speed = 5;
  TEST_ASSERT_EQUAL_STRING("Fan 5", heard(t, u));  // a new speed while running
  u = s;
  u.speed = 4;
  TEST_ASSERT_EQUAL_STRING("(nothing)", heard(s, u));  // the speed set while off isn't shown as a change
  u = t;
  u.reverse = true;
  TEST_ASSERT_EQUAL_STRING("Reverse", heard(t, u));
  TEST_ASSERT_EQUAL_STRING("Forward", heard(u, t));
  u = s;
  u.br = 40;
  TEST_ASSERT_EQUAL_STRING("Brightness 40%", heard(s, u));
  u = s;
  u.ct = 20;
  TEST_ASSERT_EQUAL_STRING("Colour 20%", heard(s, u));
  u = s;  // a light command carries brightness and colour too: the switch is what's named
  u.lightOn = false;
  u.br = 40;
  u.ct = 20;
  TEST_ASSERT_EQUAL_STRING("Light off", heard(s, u));
  fanstate::State both = t;  // light and fan on, then both off at once
  u = both;
  u.lightOn = false;
  u.fanOn = false;
  TEST_ASSERT_EQUAL_STRING("All off", heard(both, u));
}

}  // namespace

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_layout_is_sound);
  RUN_TEST(test_hit);
  RUN_TEST(test_near_misses);
  RUN_TEST(test_main_screen);
  RUN_TEST(test_light_settings_while_on);
  RUN_TEST(test_light_settings_while_off);
  RUN_TEST(test_fan_settings);
  RUN_TEST(test_labels);
  RUN_TEST(test_change_names_what_another_controller_did);
  return UNITY_END();
}
