// Touch maths (src/pt_touch): taps from raw readings, and the calibration (the cases are in pt_touch_tests.inc).
#include <unity.h>

#include <cmath>
#include <utility>
#include <vector>

#include "pt_touch.h"

using namespace pt;

#include "pt_touch_tests.inc"

void setUp() {}
void tearDown() {}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_calibration_axes_as_they_come_swapped_and_flipped);
  RUN_TEST(test_calibration_clamps_to_the_screen);
  RUN_TEST(test_taps_that_dont_spread_are_refused);
  RUN_TEST(test_a_bounced_calibration_tap_shows_as_a_large_error);
  RUN_TEST(test_a_calibration_saves_and_loads_and_refuses_damage);
  RUN_TEST(test_a_press_resting_on_the_panel_stays_bounded);
  RUN_TEST(test_a_press_becomes_one_tap_on_release);
  RUN_TEST(test_a_graze_is_ignored);
  RUN_TEST(test_a_dropout_mid_press_does_not_split_it);
  RUN_TEST(test_two_presses_are_two_taps);
  return UNITY_END();
}
