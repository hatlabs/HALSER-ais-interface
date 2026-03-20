#include <unity.h>

#include <cstring>

#include "ais/ais_signalk.h"

void setUp() {}
void tearDown() {}

// ==========================================================================
// Context string building
// ==========================================================================

void test_vessel_context() {
  char ctx[64];
  ais::build_vessel_context(477553000, ctx, sizeof(ctx));
  TEST_ASSERT_EQUAL_STRING("vessels.urn:mrn:imo:mmsi:477553000", ctx);
}

void test_vessel_context_zero_padded() {
  char ctx[64];
  ais::build_vessel_context(1234, ctx, sizeof(ctx));
  TEST_ASSERT_EQUAL_STRING("vessels.urn:mrn:imo:mmsi:000001234", ctx);
}

void test_aton_context() {
  char ctx[64];
  ais::build_aton_context(993456789, ctx, sizeof(ctx));
  TEST_ASSERT_EQUAL_STRING("atons.urn:mrn:imo:mmsi:993456789", ctx);
}

// ==========================================================================
// Class A position report
// ==========================================================================

void test_class_a_position_delta() {
  ais::ClassAPositionReport r{};
  r.message_id = 1;
  r.mmsi = 477553000;
  r.nav_status = 5;  // moored
  r.sog = 0.0;
  r.cog = 51.0;
  r.longitude = -122.345833;
  r.latitude = 47.582833;
  r.heading = 181;
  r.timestamp = 15;

  JsonDocument doc;
  ais::to_signalk_delta(r, doc);

  TEST_ASSERT_EQUAL_STRING("vessels.urn:mrn:imo:mmsi:477553000",
                           doc["context"].as<const char*>());

  JsonArray values = doc["updates"][0]["values"];
  TEST_ASSERT_TRUE(values.size() >= 4);

  // Check position
  TEST_ASSERT_EQUAL_STRING("navigation.position",
                           values[0]["path"].as<const char*>());
  TEST_ASSERT_FLOAT_WITHIN(0.001, -122.345833,
                           values[0]["value"]["longitude"].as<double>());
  TEST_ASSERT_FLOAT_WITHIN(0.001, 47.582833,
                           values[0]["value"]["latitude"].as<double>());

  // Check COG (should be in radians)
  TEST_ASSERT_EQUAL_STRING("navigation.courseOverGroundTrue",
                           values[1]["path"].as<const char*>());
  TEST_ASSERT_FLOAT_WITHIN(0.01, 51.0 * M_PI / 180.0,
                           values[1]["value"].as<double>());

  // Check SOG (should be in m/s)
  TEST_ASSERT_EQUAL_STRING("navigation.speedOverGround",
                           values[2]["path"].as<const char*>());
  TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0, values[2]["value"].as<double>());

  // Check heading (radians)
  TEST_ASSERT_EQUAL_STRING("navigation.headingTrue",
                           values[3]["path"].as<const char*>());
  TEST_ASSERT_FLOAT_WITHIN(0.01, 181.0 * M_PI / 180.0,
                           values[3]["value"].as<double>());

  // Check nav state
  TEST_ASSERT_EQUAL_STRING("navigation.state",
                           values[4]["path"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("moored",
                           values[4]["value"].as<const char*>());
}

void test_class_a_position_unavailable_heading() {
  ais::ClassAPositionReport r{};
  r.mmsi = 123456789;
  r.heading = 511;  // not available
  r.cog = 360.0;    // not available
  r.sog = NAN;      // not available
  r.latitude = 91.0;
  r.longitude = 181.0;
  r.nav_status = 15;

  JsonDocument doc;
  ais::to_signalk_delta(r, doc);

  JsonArray values = doc["updates"][0]["values"];
  // Only nav state should be present (all others unavailable)
  TEST_ASSERT_EQUAL(1, values.size());
  TEST_ASSERT_EQUAL_STRING("navigation.state",
                           values[0]["path"].as<const char*>());
}

// ==========================================================================
// Class A static/voyage data
// ==========================================================================

void test_class_a_static_delta() {
  ais::ClassAStaticData d{};
  d.mmsi = 338041751;
  d.imo = 9907;
  strncpy(d.callsign, "WTS8702", sizeof(d.callsign));
  strncpy(d.name, "ETHAN B", sizeof(d.name));
  d.ship_type = 52;
  d.to_bow = 0;
  d.to_stern = 19;
  d.to_port = 0;
  d.to_starboard = 8;
  d.draught = 2.0;
  strncpy(d.destination, "KETCHIKAN AK", sizeof(d.destination));

  JsonDocument doc;
  ais::to_signalk_delta(d, doc);

  TEST_ASSERT_EQUAL_STRING("vessels.urn:mrn:imo:mmsi:338041751",
                           doc["context"].as<const char*>());

  JsonArray values = doc["updates"][0]["values"];

  // Find name
  bool found_name = false;
  for (JsonObject v : values) {
    if (strcmp(v["path"].as<const char*>(), "name") == 0) {
      TEST_ASSERT_EQUAL_STRING("ETHAN B", v["value"].as<const char*>());
      found_name = true;
    }
  }
  TEST_ASSERT_TRUE_MESSAGE(found_name, "should have name path");

  // Find destination
  bool found_dest = false;
  for (JsonObject v : values) {
    if (strcmp(v["path"].as<const char*>(),
              "navigation.destination.commonName") == 0) {
      TEST_ASSERT_EQUAL_STRING("KETCHIKAN AK",
                               v["value"].as<const char*>());
      found_dest = true;
    }
  }
  TEST_ASSERT_TRUE_MESSAGE(found_dest, "should have destination path");

  // Find ship type
  bool found_type = false;
  for (JsonObject v : values) {
    if (strcmp(v["path"].as<const char*>(), "design.aisShipType") == 0) {
      TEST_ASSERT_EQUAL(52, v["value"].as<int>());
      found_type = true;
    }
  }
  TEST_ASSERT_TRUE_MESSAGE(found_type, "should have ship type path");
}

// ==========================================================================
// Class B position report
// ==========================================================================

void test_class_b_position_delta() {
  ais::ClassBPositionReport r{};
  r.mmsi = 272016004;
  r.sog = 11.5;
  r.longitude = 31.994937;
  r.latitude = 63.60296;
  r.cog = 208.1;
  r.heading = 511;  // not available

  JsonDocument doc;
  ais::to_signalk_delta(r, doc);

  TEST_ASSERT_EQUAL_STRING("vessels.urn:mrn:imo:mmsi:272016004",
                           doc["context"].as<const char*>());

  JsonArray values = doc["updates"][0]["values"];
  // Position, COG, SOG (no heading since 511)
  TEST_ASSERT_EQUAL(3, values.size());
}

// ==========================================================================
// AtoN report
// ==========================================================================

void test_aton_delta_uses_aton_context() {
  ais::AtoNReport r{};
  r.mmsi = 993456789;
  r.aton_type = 15;
  strncpy(r.name, "TEST BUOY", sizeof(r.name));
  r.latitude = 60.0;
  r.longitude = 25.0;

  JsonDocument doc;
  ais::to_signalk_delta(r, doc);

  // AtoN should use "atons." context, not "vessels."
  TEST_ASSERT_EQUAL_STRING("atons.urn:mrn:imo:mmsi:993456789",
                           doc["context"].as<const char*>());
}

// ==========================================================================
// JSON serialization roundtrip
// ==========================================================================

void test_delta_serializes_to_valid_json() {
  ais::ClassAPositionReport r{};
  r.mmsi = 477553000;
  r.sog = 5.0;
  r.cog = 90.0;
  r.longitude = 25.0;
  r.latitude = 60.0;
  r.heading = 90;
  r.nav_status = 0;

  JsonDocument doc;
  ais::to_signalk_delta(r, doc);

  char output[1024];
  serializeJson(doc, output, sizeof(output));

  // Parse it back
  JsonDocument parsed;
  auto err = deserializeJson(parsed, output);
  TEST_ASSERT_TRUE_MESSAGE(err == DeserializationError::Ok,
                           "should produce valid JSON");
  TEST_ASSERT_TRUE(parsed["context"].is<const char*>());
  TEST_ASSERT_TRUE(parsed["updates"].is<JsonArray>());
}

// ==========================================================================
// Test runner
// ==========================================================================

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_vessel_context);
  RUN_TEST(test_vessel_context_zero_padded);
  RUN_TEST(test_aton_context);

  RUN_TEST(test_class_a_position_delta);
  RUN_TEST(test_class_a_position_unavailable_heading);

  RUN_TEST(test_class_a_static_delta);

  RUN_TEST(test_class_b_position_delta);

  RUN_TEST(test_aton_delta_uses_aton_context);

  RUN_TEST(test_delta_serializes_to_valid_json);

  return UNITY_END();
}
