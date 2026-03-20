#include <unity.h>

#include <cmath>
#include <cstring>

// Include implementation directly for native test linking
#include "ais/ais_decoder.cpp"

// Helper: assert floating point values within tolerance
#define TEST_ASSERT_FLOAT_WITHIN_MSG(delta, expected, actual, msg) \
  TEST_ASSERT_FLOAT_WITHIN_MESSAGE(delta, expected, actual, msg)

// ==========================================================================
// 6-bit character decoding
// ==========================================================================

void test_decode_armored_char_zero() {
  // '0' (ASCII 48) → 0
  TEST_ASSERT_EQUAL_INT(0, ais::decode_armored_char('0'));
}

void test_decode_armored_char_w() {
  // 'w' (ASCII 119) → 119-48-8 = 63
  TEST_ASSERT_EQUAL_INT(63, ais::decode_armored_char('w'));
  // 'W' (ASCII 87) → 87-48 = 39
  TEST_ASSERT_EQUAL_INT(39, ais::decode_armored_char('W'));
}

void test_decode_armored_char_at_boundary() {
  // 'W' (ASCII 87) → 87-48=39... but chars above 87 need +8 adjustment
  // Actually: ASCII 48-87 map to 0-39, ASCII 96-119 map to 40-63
  // '`' (ASCII 96) → 40
  TEST_ASSERT_EQUAL_INT(40, ais::decode_armored_char('`'));
}

void test_decode_armored_char_invalid() {
  TEST_ASSERT_EQUAL_INT(-1, ais::decode_armored_char(0x20));  // space
  TEST_ASSERT_EQUAL_INT(-1, ais::decode_armored_char(0x7F));  // DEL
}

// ==========================================================================
// Bitstream extraction
// ==========================================================================

void test_extract_uint_simple() {
  // Payload "1" = 6-bit value 1 = 000001
  uint8_t payload[] = {1};
  TEST_ASSERT_EQUAL_UINT32(0, ais::extract_uint(payload, 0, 3));  // bits 0-2
  TEST_ASSERT_EQUAL_UINT32(1, ais::extract_uint(payload, 3, 3));  // bits 3-5
}

void test_extract_uint_cross_boundary() {
  // Two 6-bit values: 0b111111, 0b000001 → bits: 111111 000001
  uint8_t payload[] = {0x3F, 0x01};
  // bits 4-9 = 11|0000 = 48 (crosses 6-bit boundary)
  TEST_ASSERT_EQUAL_UINT32(48, ais::extract_uint(payload, 4, 6));
}

void test_extract_int_positive() {
  // 6-bit value 15 = 001111 → as 6-bit signed = +15
  uint8_t payload[] = {15};
  TEST_ASSERT_EQUAL_INT32(15, ais::extract_int(payload, 0, 6));
}

void test_extract_int_negative() {
  // 6-bit value 0b110001 = 49 → as 6-bit signed = -15
  uint8_t payload[] = {49};
  TEST_ASSERT_EQUAL_INT32(-15, ais::extract_int(payload, 0, 6));
}

// ==========================================================================
// String extraction
// ==========================================================================

void test_extract_string_basic() {
  // 'H', 'I' in 6-bit AIS encoding:
  // 'H' = 8+32 = 40... Actually AIS 6-bit text:
  // 0 = '@', 1 = 'A', ..., 31 = '_' (space-like), 32 = ' ', ...
  // So value 8 = 'H', value 9 = 'I'
  uint8_t payload[] = {8, 9};
  char out[3];
  ais::extract_string(payload, 0, 2, out);
  TEST_ASSERT_EQUAL_STRING("HI", out);
}

void test_extract_string_strips_trailing_at() {
  // '@' is the padding character (value 0)
  uint8_t payload[] = {8, 9, 0, 0};
  char out[5];
  ais::extract_string(payload, 0, 4, out);
  TEST_ASSERT_EQUAL_STRING("HI", out);
}

// ==========================================================================
// Type 1/2/3: Class A Position Report
// ==========================================================================

// Reference: decoded with pyais
// !AIVDM,1,1,,B,177KQJ5000G?tO`K>RA1wUbN0TKH,0*5C
// msg_type: 1, mmsi: 477553000, status: 5, turn: 0.0, speed: 0.0,
// accuracy: False, lon: -122.345833, lat: 47.582833, course: 51.0,
// heading: 181, second: 15, raim: False
void test_decode_type1_position_report() {
  const char* payload = "177KQJ5000G?tO`K>RA1wUbN0TKH";
  auto result = ais::decode_payload(payload, 0);
  TEST_ASSERT_TRUE_MESSAGE(result.has_value(), "decode should succeed");

  auto* report = std::get_if<ais::ClassAPositionReport>(&result.value());
  TEST_ASSERT_NOT_NULL_MESSAGE(report, "should be ClassAPositionReport");

  TEST_ASSERT_EQUAL_UINT8(1, report->message_id);
  TEST_ASSERT_EQUAL_UINT32(477553000, report->mmsi);
  TEST_ASSERT_EQUAL_UINT8(5, report->nav_status);
  TEST_ASSERT_FLOAT_WITHIN(0.1, 0.0, report->rot);
  TEST_ASSERT_FLOAT_WITHIN(0.01, 0.0, report->sog);
  TEST_ASSERT_FALSE(report->accuracy);
  TEST_ASSERT_FLOAT_WITHIN(0.001, -122.345833, report->longitude);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 47.582833, report->latitude);
  TEST_ASSERT_FLOAT_WITHIN(0.1, 51.0, report->cog);
  TEST_ASSERT_EQUAL_UINT16(181, report->heading);
  TEST_ASSERT_EQUAL_UINT8(15, report->timestamp);
  TEST_ASSERT_FALSE(report->raim);
}

// !AIVDM,1,1,,B,35PLMB5000De@8d<<V6P0Ge60000,0*1C
// msg_type: 3, mmsi: 369565000, status: 5, turn: 0.0, speed: 0.0,
// accuracy: False, lon: -157.886683, lat: 21.315457, course: 0.1,
// heading: 246, second: 35, raim: False
void test_decode_type3_position_report() {
  const char* payload = "35PLMB5000De@8d<<V6P0Ge60000";
  auto result = ais::decode_payload(payload, 0);
  TEST_ASSERT_TRUE_MESSAGE(result.has_value(), "decode should succeed");

  auto* report = std::get_if<ais::ClassAPositionReport>(&result.value());
  TEST_ASSERT_NOT_NULL_MESSAGE(report, "should be ClassAPositionReport");

  TEST_ASSERT_EQUAL_UINT8(3, report->message_id);
  TEST_ASSERT_EQUAL_UINT32(369565000, report->mmsi);
  TEST_ASSERT_EQUAL_UINT8(5, report->nav_status);
  TEST_ASSERT_FLOAT_WITHIN(0.001, -157.886683, report->longitude);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 21.315457, report->latitude);
  TEST_ASSERT_FLOAT_WITHIN(0.1, 0.1, report->cog);
  TEST_ASSERT_EQUAL_UINT16(246, report->heading);
  TEST_ASSERT_EQUAL_UINT8(35, report->timestamp);
}

// ==========================================================================
// Type 5: Class A Static and Voyage Data
// ==========================================================================

// Multi-part payload (already reassembled by caller):
// Part 1: 552HKUh009c=MA?SO38E@P4r080000000000000l00C
// Part 2: 086s@052iE0j2BhC`0Bh00000000
// Combined payload:
// msg_type: 5, mmsi: 338041751, imo: 9907, callsign: WTS8702,
// shipname: ETHAN B, ship_type: 52, to_bow: 0, to_stern: 19,
// to_port: 0, to_starboard: 8, epfd: 1, month: 11, day: 22,
// hour: 16, minute: 0, draught: 2.0, destination: KETCHIKAN AK
void test_decode_type5_static_data() {
  const char* payload =
      "552HKUh009c=MA?SO38E@P4r080000000000000l00C"
      "086s@052iE0j2BhC`0Bh00000000";
  auto result = ais::decode_payload(payload, 2);
  TEST_ASSERT_TRUE_MESSAGE(result.has_value(), "decode should succeed");

  auto* data = std::get_if<ais::ClassAStaticData>(&result.value());
  TEST_ASSERT_NOT_NULL_MESSAGE(data, "should be ClassAStaticData");

  TEST_ASSERT_EQUAL_UINT8(5, data->message_id);
  TEST_ASSERT_EQUAL_UINT32(338041751, data->mmsi);
  TEST_ASSERT_EQUAL_UINT32(9907, data->imo);
  TEST_ASSERT_EQUAL_STRING("WTS8702", data->callsign);
  TEST_ASSERT_EQUAL_STRING("ETHAN B", data->name);
  TEST_ASSERT_EQUAL_UINT8(52, data->ship_type);
  TEST_ASSERT_EQUAL_UINT16(0, data->to_bow);
  TEST_ASSERT_EQUAL_UINT16(19, data->to_stern);
  TEST_ASSERT_EQUAL_UINT16(0, data->to_port);
  TEST_ASSERT_EQUAL_UINT16(8, data->to_starboard);
  TEST_ASSERT_EQUAL_UINT8(1, data->epfd);
  TEST_ASSERT_EQUAL_UINT8(11, data->month);
  TEST_ASSERT_EQUAL_UINT8(22, data->day);
  TEST_ASSERT_EQUAL_UINT8(16, data->hour);
  TEST_ASSERT_EQUAL_UINT8(0, data->minute);
  TEST_ASSERT_FLOAT_WITHIN(0.1, 2.0, data->draught);
  TEST_ASSERT_EQUAL_STRING("KETCHIKAN AK", data->destination);
}

// ==========================================================================
// Type 18: Class B Position Report
// ==========================================================================

// !AIVDM,1,1,,B,B43JRQ00LhTWHA96CG226wo60000,0*51
// pyais: msg_type: 18, mmsi: 272016004, speed: 11.5, accuracy: False,
// lon: 31.994937, lat: 63.60296, course: 208.1, heading: 383,
// second: 46, cs: True, display: True, dsc: False, band: False,
// msg22: False, assigned: False, raim: False
void test_decode_type18_class_b_position() {
  const char* payload = "B43JRQ00LhTWHA96CG226wo60000";
  auto result = ais::decode_payload(payload, 0);
  TEST_ASSERT_TRUE_MESSAGE(result.has_value(), "decode should succeed");

  auto* report = std::get_if<ais::ClassBPositionReport>(&result.value());
  TEST_ASSERT_NOT_NULL_MESSAGE(report, "should be ClassBPositionReport");

  TEST_ASSERT_EQUAL_UINT8(18, report->message_id);
  TEST_ASSERT_EQUAL_UINT32(272016004, report->mmsi);
  TEST_ASSERT_FLOAT_WITHIN(0.1, 11.5, report->sog);
  TEST_ASSERT_FALSE(report->accuracy);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 31.994937, report->longitude);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 63.60296, report->latitude);
  TEST_ASSERT_FLOAT_WITHIN(0.1, 208.1, report->cog);
  TEST_ASSERT_EQUAL_UINT16(383, report->heading);
  TEST_ASSERT_EQUAL_UINT8(46, report->timestamp);
  TEST_ASSERT_TRUE(report->cs_unit);
  TEST_ASSERT_TRUE(report->display);
  TEST_ASSERT_FALSE(report->dsc);
  TEST_ASSERT_FALSE(report->raim);
}

// ==========================================================================
// Invalid/edge cases
// ==========================================================================

void test_decode_empty_payload() {
  auto result = ais::decode_payload("", 0);
  TEST_ASSERT_FALSE_MESSAGE(result.has_value(),
                            "empty payload should fail");
}

void test_decode_too_short_payload() {
  auto result = ais::decode_payload("1", 0);
  TEST_ASSERT_FALSE_MESSAGE(result.has_value(),
                            "too short payload should fail");
}

void test_decode_unknown_message_type() {
  // Message type 0 is not a valid AIS message type
  // '0' = 6-bit value 0, so payload starting with '0' = msg type 0
  auto result = ais::decode_payload("0000000000000000000000000000", 0);
  TEST_ASSERT_FALSE_MESSAGE(result.has_value(),
                            "unknown msg type should fail");
}

// ==========================================================================
// Test runner
// ==========================================================================

void setUp() {}
void tearDown() {}

int main() {
  UNITY_BEGIN();

  // 6-bit decoding
  RUN_TEST(test_decode_armored_char_zero);
  RUN_TEST(test_decode_armored_char_w);
  RUN_TEST(test_decode_armored_char_at_boundary);
  RUN_TEST(test_decode_armored_char_invalid);

  // Bitstream extraction
  RUN_TEST(test_extract_uint_simple);
  RUN_TEST(test_extract_uint_cross_boundary);
  RUN_TEST(test_extract_int_positive);
  RUN_TEST(test_extract_int_negative);

  // String extraction
  RUN_TEST(test_extract_string_basic);
  RUN_TEST(test_extract_string_strips_trailing_at);

  // Type 1/2/3 decoding
  RUN_TEST(test_decode_type1_position_report);
  RUN_TEST(test_decode_type3_position_report);

  // Type 5 decoding
  RUN_TEST(test_decode_type5_static_data);

  // Type 18 decoding
  RUN_TEST(test_decode_type18_class_b_position);

  // Edge cases
  RUN_TEST(test_decode_empty_payload);
  RUN_TEST(test_decode_too_short_payload);
  RUN_TEST(test_decode_unknown_message_type);

  return UNITY_END();
}
