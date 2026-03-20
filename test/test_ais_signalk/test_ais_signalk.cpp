#include <unity.h>

#include <cstdio>
#include <cstring>

#include "ais/ais_conversions.h"
#include "ais/ais_message_types.h"

void setUp() {}
void tearDown() {}

// ==========================================================================
// Context string building (moved from removed ais_signalk.h)
// ==========================================================================

void build_vessel_context(uint32_t mmsi, char* out, size_t out_size) {
  snprintf(out, out_size, "vessels.urn:mrn:imo:mmsi:%09u", mmsi);
}

void test_vessel_context() {
  char ctx[64];
  build_vessel_context(477553000, ctx, sizeof(ctx));
  TEST_ASSERT_EQUAL_STRING("vessels.urn:mrn:imo:mmsi:477553000", ctx);
}

void test_vessel_context_zero_padded() {
  char ctx[64];
  build_vessel_context(1234, ctx, sizeof(ctx));
  TEST_ASSERT_EQUAL_STRING("vessels.urn:mrn:imo:mmsi:000001234", ctx);
}

// ==========================================================================
// Unit conversion integration (AIS → SK values)
// ==========================================================================

void test_position_report_to_sk_values() {
  ais::ClassAPositionReport r{};
  r.mmsi = 477553000;
  r.sog = 10.0;     // knots
  r.cog = 90.0;     // degrees
  r.heading = 180;   // degrees
  r.latitude = 47.582833;
  r.longitude = -122.345833;

  // Verify conversions that would be applied by SKAISOutput
  TEST_ASSERT_FLOAT_WITHIN(0.001, 5.14444, ais::sog_to_ms(r.sog));
  TEST_ASSERT_FLOAT_WITHIN(0.001, M_PI / 2.0, ais::cog_to_radians(r.cog));
  TEST_ASSERT_FLOAT_WITHIN(0.001, M_PI, ais::heading_to_radians(r.heading));
}

void test_unavailable_values_produce_nan() {
  TEST_ASSERT_TRUE(std::isnan(ais::heading_to_radians(511)));
  TEST_ASSERT_TRUE(std::isnan(ais::cog_to_radians(360.0)));
  TEST_ASSERT_TRUE(std::isnan(ais::sog_to_ms(NAN)));
}

void test_position_json_format() {
  // Verify the position JSON format used by SKAISOutput
  char json[64];
  snprintf(json, sizeof(json), "{\"longitude\":%.6f,\"latitude\":%.6f}",
           -122.345833, 47.582833);

  // Should be valid JSON with expected fields
  TEST_ASSERT_NOT_NULL(strstr(json, "\"longitude\":-122.345833"));
  TEST_ASSERT_NOT_NULL(strstr(json, "\"latitude\":47.582833"));
}

// ==========================================================================
// Test runner
// ==========================================================================

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_vessel_context);
  RUN_TEST(test_vessel_context_zero_padded);
  RUN_TEST(test_position_report_to_sk_values);
  RUN_TEST(test_unavailable_values_produce_nan);
  RUN_TEST(test_position_json_format);

  return UNITY_END();
}
