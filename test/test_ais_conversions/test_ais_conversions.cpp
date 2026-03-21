#include <unity.h>

#include <cmath>

#include "ais/ais_conversions.h"

void setUp() {}
void tearDown() {}

// ==========================================================================
// Heading conversion
// ==========================================================================

void test_heading_zero() {
  TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0, ais::heading_to_radians(0));
}

void test_heading_180() {
  TEST_ASSERT_FLOAT_WITHIN(0.001, M_PI, ais::heading_to_radians(180));
}

void test_heading_359() {
  double expected = 359.0 * M_PI / 180.0;
  TEST_ASSERT_FLOAT_WITHIN(0.001, expected, ais::heading_to_radians(359));
}

void test_heading_not_available() {
  TEST_ASSERT_TRUE(std::isnan(ais::heading_to_radians(511)));
}

void test_heading_360_not_available() {
  TEST_ASSERT_TRUE(std::isnan(ais::heading_to_radians(360)));
}

// ==========================================================================
// COG conversion
// ==========================================================================

void test_cog_zero() {
  TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0, ais::cog_to_radians(0.0));
}

void test_cog_90() {
  TEST_ASSERT_FLOAT_WITHIN(0.001, M_PI / 2.0, ais::cog_to_radians(90.0));
}

void test_cog_not_available() {
  TEST_ASSERT_TRUE(std::isnan(ais::cog_to_radians(360.0)));
}

// ==========================================================================
// SOG conversion
// ==========================================================================

void test_sog_zero() {
  TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0, ais::sog_to_ms(0.0));
}

void test_sog_10_knots() {
  // 10 knots = 10 * 1852/3600 = 5.14444 m/s
  TEST_ASSERT_FLOAT_WITHIN(0.001, 5.14444, ais::sog_to_ms(10.0));
}

void test_sog_nan() {
  TEST_ASSERT_TRUE(std::isnan(ais::sog_to_ms(NAN)));
}

// ==========================================================================
// ROT conversion
// ==========================================================================

void test_rot_zero() {
  TEST_ASSERT_FLOAT_WITHIN(0.0001, 0.0, ais::rot_to_rads(0.0));
}

void test_rot_positive() {
  // 60 deg/min = 1 deg/s = PI/180 rad/s ≈ 0.01745
  TEST_ASSERT_FLOAT_WITHIN(0.0001, M_PI / 180.0, ais::rot_to_rads(60.0));
}

void test_rot_nan() {
  TEST_ASSERT_TRUE(std::isnan(ais::rot_to_rads(NAN)));
}

// ==========================================================================
// Dimensions conversion
// ==========================================================================

void test_dimensions_basic() {
  // bow=10, stern=20, port=5, starboard=3
  auto dims = ais::dimensions_to_n2k(10, 20, 5, 3);
  TEST_ASSERT_FLOAT_WITHIN(0.1, 30.0, dims.length);      // 10+20
  TEST_ASSERT_FLOAT_WITHIN(0.1, 8.0, dims.beam);          // 5+3
  TEST_ASSERT_FLOAT_WITHIN(0.1, 3.0, dims.pos_ref_stbd);  // starboard
  TEST_ASSERT_FLOAT_WITHIN(0.1, 10.0, dims.pos_ref_bow);  // bow
}

void test_dimensions_zero() {
  auto dims = ais::dimensions_to_n2k(0, 19, 0, 8);
  TEST_ASSERT_FLOAT_WITHIN(0.1, 19.0, dims.length);
  TEST_ASSERT_FLOAT_WITHIN(0.1, 8.0, dims.beam);
  TEST_ASSERT_FLOAT_WITHIN(0.1, 8.0, dims.pos_ref_stbd);
  TEST_ASSERT_FLOAT_WITHIN(0.1, 0.0, dims.pos_ref_bow);
}

// ==========================================================================
// ETA conversion
// ==========================================================================

void test_eta_not_available() {
  auto eta = ais::eta_to_n2k(0, 0, 24, 60, 2026);
  TEST_ASSERT_EQUAL_UINT16(0, eta.date);
  TEST_ASSERT_FLOAT_WITHIN(0.1, 0.0, eta.time);
}

void test_eta_time_calculation() {
  auto eta = ais::eta_to_n2k(1, 1, 16, 30, 2026);
  // 16:30 = 16*3600 + 30*60 = 59400 seconds
  TEST_ASSERT_FLOAT_WITHIN(0.1, 59400.0, eta.time);
}

void test_eta_date_jan1_2026() {
  auto eta = ais::eta_to_n2k(1, 1, 0, 0, 2026);
  // Days from 1970-01-01 to 2026-01-01
  // 1970-2026 = 56 years, 14 leap years (72,76,80,84,88,92,96,00,04,08,12,16,20,24)
  // = 56*365 + 14 = 20440 + 14 = 20454
  // Jan 1 = day 0 offset → 20454
  TEST_ASSERT_EQUAL_UINT16(20454, eta.date);
}

// ==========================================================================
// Test runner
// ==========================================================================

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_heading_zero);
  RUN_TEST(test_heading_180);
  RUN_TEST(test_heading_359);
  RUN_TEST(test_heading_not_available);
  RUN_TEST(test_heading_360_not_available);

  RUN_TEST(test_cog_zero);
  RUN_TEST(test_cog_90);
  RUN_TEST(test_cog_not_available);

  RUN_TEST(test_sog_zero);
  RUN_TEST(test_sog_10_knots);
  RUN_TEST(test_sog_nan);

  RUN_TEST(test_rot_zero);
  RUN_TEST(test_rot_positive);
  RUN_TEST(test_rot_nan);

  RUN_TEST(test_dimensions_basic);
  RUN_TEST(test_dimensions_zero);

  RUN_TEST(test_eta_not_available);
  RUN_TEST(test_eta_time_calculation);
  RUN_TEST(test_eta_date_jan1_2026);

  return UNITY_END();
}
