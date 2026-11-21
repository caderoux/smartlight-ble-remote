// ---------------------------------------------------------------- touch

static const int W = 320, H = 240;
static const int TARGETS[5][2] = {{20, 20}, {300, 20}, {20, 220}, {300, 220}, {160, 120}};

// A fake panel: the raw reading a touch at (x, y) gives. Raw spans ~200..3900 like the real XPT2046.
static Raw panel(int x, int y, bool swap, bool flipX = false, bool flipY = false) {
  int u = int(std::lround(200 + (flipX ? W - 1 - x : x) * 3700.0 / (W - 1)));
  int v = int(std::lround(200 + (flipY ? H - 1 - y : y) * 3700.0 / (H - 1)));
  return swap ? Raw{v, u} : Raw{u, v};
}

static std::vector<Target> taps(bool swap, bool flipX = false, bool flipY = false) {
  std::vector<Target> out;
  for (auto &t : TARGETS) out.push_back(Target{panel(t[0], t[1], swap, flipX, flipY), t[0], t[1]});
  return out;
}

static void checkFit(bool swap, bool flipX, bool flipY) {
  Calibration c;
  TEST_ASSERT_TRUE(Calibration::fit(taps(swap, flipX, flipY), W, H, c));
  TEST_ASSERT_EQUAL(swap, c.swap);
  const int extra[][2] = {{100, 60}, {250, 200}};
  for (auto &t : TARGETS) {
    int x, y;
    c.toScreen(panel(t[0], t[1], swap, flipX, flipY), x, y);
    TEST_ASSERT_INT_WITHIN(1, t[0], x);
    TEST_ASSERT_INT_WITHIN(1, t[1], y);
  }
  for (auto &t : extra) {
    int x, y;
    c.toScreen(panel(t[0], t[1], swap, flipX, flipY), x, y);
    TEST_ASSERT_INT_WITHIN(1, t[0], x);
    TEST_ASSERT_INT_WITHIN(1, t[1], y);
  }
}

void test_calibration_axes_as_they_come_swapped_and_flipped() {
  checkFit(false, false, false);
  checkFit(true, false, false);
  checkFit(true, true, false);
  checkFit(false, false, true);
}

void test_calibration_clamps_to_the_screen() {
  Calibration c;
  Calibration::fit(taps(false), W, H, c);
  int x, y;
  c.toScreen(Raw{0, 0}, x, y);
  TEST_ASSERT_EQUAL(0, x);
  TEST_ASSERT_EQUAL(0, y);
  c.toScreen(Raw{4095, 4095}, x, y);
  TEST_ASSERT_EQUAL(W - 1, x);
  TEST_ASSERT_EQUAL(H - 1, y);
}

void test_taps_that_dont_spread_are_refused() {
  std::vector<Target> same;
  for (auto &t : TARGETS) same.push_back(Target{Raw{1000, 1000}, t[0], t[1]});
  Calibration c;
  TEST_ASSERT_FALSE(Calibration::fit(same, W, H, c));
}

void test_a_bounced_calibration_tap_shows_as_a_large_error() {
  auto good = taps(true);
  Calibration c;
  Calibration::fit(good, W, H, c);
  TEST_ASSERT_LESS_OR_EQUAL(1, c.worstError(good));
  auto bad = good;
  bad[1].raw = bad[0].raw;  // target 2 got target 1's reading
  Calibration::fit(bad, W, H, c);
  TEST_ASSERT_GREATER_THAN(20, c.worstError(bad));
}

void test_a_calibration_saves_and_loads_and_refuses_damage() {
  Calibration c;
  Calibration::fit(taps(true), W, H, c);
  Calibration back;
  TEST_ASSERT_TRUE(back.parse(c.toString()));
  TEST_ASSERT_EQUAL(c.swap, back.swap);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, c.ax, back.ax);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, c.bx, back.bx);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, c.ay, back.ay);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, c.by, back.by);
  Calibration bad;
  TEST_ASSERT_FALSE(bad.parse(""));
  TEST_ASSERT_FALSE(bad.parse("1,0.08"));              // too few fields
  TEST_ASSERT_FALSE(bad.parse("1,2,3,4,5junk"));       // trailing junk
  TEST_ASSERT_FALSE(bad.parse("2,0.08,1,0.06,1"));     // swap isn't 0 or 1
  TEST_ASSERT_FALSE(bad.parse("1,0,1,0.06,1"));        // a zero slope
  TEST_ASSERT_FALSE(bad.parse("1,nan,1,0.06,1"));
}

void test_a_press_resting_on_the_panel_stays_bounded() {
  Taps t;
  Raw tap;
  for (int i = 0; i < 100000; i++) t.feed(true, Raw{500, 600}, tap);  // ~35 minutes of a hand resting on it
  t.feed(false, {}, tap);
  t.feed(false, {}, tap);
  TEST_ASSERT_TRUE(t.feed(false, {}, tap));
  TEST_ASSERT_EQUAL(500, tap.x);
}

static std::vector<Raw> feedAll(const std::vector<std::pair<bool, Raw>> &readings) {
  Taps t;
  std::vector<Raw> out;
  for (auto &r : readings) {
    Raw tap;
    if (t.feed(r.first, r.second, tap)) out.push_back(tap);
  }
  return out;
}

void test_a_press_becomes_one_tap_on_release() {
  auto got = feedAll({{true, {100, 200}}, {true, {102, 198}}, {true, {98, 201}}, {true, {300, 900}},
                      {false, {}}, {false, {}}, {false, {}}});
  TEST_ASSERT_EQUAL(1, int(got.size()));
  TEST_ASSERT_EQUAL(102, got[0].x);
  TEST_ASSERT_EQUAL(201, got[0].y);  // even count: the upper middle
}

void test_a_graze_is_ignored() {
  TEST_ASSERT_EQUAL(0, int(feedAll({{true, {1, 1}}, {true, {1, 1}}, {false, {}}, {false, {}}, {false, {}}}).size()));
}

void test_a_dropout_mid_press_does_not_split_it() {
  std::vector<std::pair<bool, Raw>> r;
  for (int i = 0; i < 3; i++) r.push_back({true, {10, 10}});
  r.push_back({false, {}});
  r.push_back({false, {}});
  for (int i = 0; i < 3; i++) r.push_back({true, {10, 10}});
  for (int i = 0; i < 3; i++) r.push_back({false, {}});
  TEST_ASSERT_EQUAL(1, int(feedAll(r).size()));
}

void test_two_presses_are_two_taps() {
  std::vector<std::pair<bool, Raw>> r;
  for (int k = 0; k < 2; k++) {
    for (int i = 0; i < 3; i++) r.push_back({true, {10, 10}});
    for (int i = 0; i < 3; i++) r.push_back({false, {}});
  }
  TEST_ASSERT_EQUAL(2, int(feedAll(r).size()));
}
