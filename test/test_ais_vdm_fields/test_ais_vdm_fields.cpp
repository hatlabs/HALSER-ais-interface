#include <unity.h>

#include <cstring>

// Include implementations for native linking
#include "ais/ais_decoder.cpp"
#include "ais/ais_reassembler.cpp"
#include "ais/ais_vdm_fields.h"

void setUp() {}
void tearDown() {}

// Helper: build null-separated field string and offsets from a VDM sentence.
// Simulates what SentenceParser::parse() does: splits by commas.
// sentence: just the fields part (after "!AIVDM," and before "*xx")
// e.g. "1,1,,B,177KQJ5000G?tO`K>RA1wUbN0TKH,0"
struct ParsedFields {
  char buffer[512];
  int offsets[10];
  int count;
};

ParsedFields split_fields(const char* fields_str) {
  ParsedFields result{};
  strncpy(result.buffer, fields_str, sizeof(result.buffer) - 1);
  result.count = 0;
  result.offsets[0] = 0;

  for (int i = 0; result.buffer[i] != '\0'; i++) {
    if (result.buffer[i] == ',') {
      result.buffer[i] = '\0';
      result.count++;
      if (result.count < 10) {
        result.offsets[result.count] = i + 1;
      }
    }
  }
  result.count++;  // last field
  return result;
}

// ==========================================================================
// Single-part Type 1 message
// ==========================================================================

void test_vdm_fields_type1_single_part() {
  // Fields from: !AIVDM,1,1,,B,177KQJ5000G?tO`K>RA1wUbN0TKH,0*5C
  auto f = split_fields("1,1,,B,177KQJ5000G?tO`K>RA1wUbN0TKH,0");
  ais::AISReassembler reassembler;

  auto result =
      ais::parse_vdm_fields(f.buffer, f.offsets, f.count, reassembler, 1000);
  TEST_ASSERT_TRUE_MESSAGE(result.has_value(), "should succeed");

  // Should be an AISMessage, not VDMParseIncomplete
  auto* msg = std::get_if<ais::AISMessage>(&result.value());
  TEST_ASSERT_NOT_NULL_MESSAGE(msg, "should be AISMessage");

  auto* report = std::get_if<ais::ClassAPositionReport>(msg);
  TEST_ASSERT_NOT_NULL_MESSAGE(report, "should be ClassAPositionReport");

  TEST_ASSERT_EQUAL_UINT8(1, report->message_id);
  TEST_ASSERT_EQUAL_UINT32(477553000, report->mmsi);
  TEST_ASSERT_FLOAT_WITHIN(0.001, -122.345833, report->longitude);
  TEST_ASSERT_FLOAT_WITHIN(0.001, 47.582833, report->latitude);
}

// ==========================================================================
// Multi-part Type 5 message
// ==========================================================================

void test_vdm_fields_type5_first_fragment() {
  // Fragment 1 of 2: !AIVDM,2,1,0,B,552HKUh009c=MA?SO38E@P4r080000000000000l00C,0*5D
  auto f = split_fields(
      "2,1,0,B,552HKUh009c=MA?SO38E@P4r080000000000000l00C,0");
  ais::AISReassembler reassembler;

  auto result =
      ais::parse_vdm_fields(f.buffer, f.offsets, f.count, reassembler, 1000);
  TEST_ASSERT_TRUE_MESSAGE(result.has_value(), "fragment 1 should succeed");

  // Should be VDMParseIncomplete (not yet complete)
  auto* incomplete =
      std::get_if<ais::VDMParseIncomplete>(&result.value());
  TEST_ASSERT_NOT_NULL_MESSAGE(incomplete, "should be VDMParseIncomplete");
}

void test_vdm_fields_type5_complete() {
  ais::AISReassembler reassembler;

  // Fragment 1
  auto f1 = split_fields(
      "2,1,0,B,552HKUh009c=MA?SO38E@P4r080000000000000l00C,0");
  ais::parse_vdm_fields(f1.buffer, f1.offsets, f1.count, reassembler, 1000);

  // Fragment 2
  auto f2 = split_fields("2,2,0,B,086s@052iE0j2BhC`0Bh00000000,2");
  auto result =
      ais::parse_vdm_fields(f2.buffer, f2.offsets, f2.count, reassembler, 1001);
  TEST_ASSERT_TRUE_MESSAGE(result.has_value(), "fragment 2 should complete");

  auto* msg = std::get_if<ais::AISMessage>(&result.value());
  TEST_ASSERT_NOT_NULL_MESSAGE(msg, "should be AISMessage");

  auto* data = std::get_if<ais::ClassAStaticData>(msg);
  TEST_ASSERT_NOT_NULL_MESSAGE(data, "should be ClassAStaticData");

  TEST_ASSERT_EQUAL_UINT32(338041751, data->mmsi);
  TEST_ASSERT_EQUAL_STRING("WTS8702", data->callsign);
  TEST_ASSERT_EQUAL_STRING("ETHAN B", data->name);
  TEST_ASSERT_EQUAL_STRING("KETCHIKAN AK", data->destination);
}

// ==========================================================================
// Type 18 Class B position
// ==========================================================================

void test_vdm_fields_type18() {
  auto f = split_fields("1,1,,B,B43JRQ00LhTWHA96CG226wo60000,0");
  ais::AISReassembler reassembler;

  auto result =
      ais::parse_vdm_fields(f.buffer, f.offsets, f.count, reassembler, 1000);
  TEST_ASSERT_TRUE(result.has_value());

  auto* msg = std::get_if<ais::AISMessage>(&result.value());
  TEST_ASSERT_NOT_NULL(msg);

  auto* report = std::get_if<ais::ClassBPositionReport>(msg);
  TEST_ASSERT_NOT_NULL_MESSAGE(report, "should be ClassBPositionReport");
  TEST_ASSERT_EQUAL_UINT32(272016004, report->mmsi);
}

// ==========================================================================
// Error cases
// ==========================================================================

void test_vdm_fields_too_few_fields() {
  auto f = split_fields("1,1,,B,payload");  // only 5 fields
  ais::AISReassembler reassembler;

  auto result =
      ais::parse_vdm_fields(f.buffer, f.offsets, f.count, reassembler, 1000);
  TEST_ASSERT_FALSE_MESSAGE(result.has_value(), "too few fields should fail");
}

void test_vdm_fields_invalid_payload() {
  // Valid field count but garbage payload
  auto f = split_fields("1,1,,B,!!!invalid!!!,0");
  ais::AISReassembler reassembler;

  auto result =
      ais::parse_vdm_fields(f.buffer, f.offsets, f.count, reassembler, 1000);
  TEST_ASSERT_FALSE_MESSAGE(result.has_value(),
                            "invalid payload should fail");
}

void test_vdm_fields_fragment1_without_fragment2() {
  // First fragment of a multi-part message returns incomplete
  auto f = split_fields(
      "2,1,3,A,552HKUh009c=MA?SO38E@P4r080000000000000l00C,0");
  ais::AISReassembler reassembler;

  auto result =
      ais::parse_vdm_fields(f.buffer, f.offsets, f.count, reassembler, 1000);
  TEST_ASSERT_TRUE_MESSAGE(result.has_value(),
                           "first fragment should return incomplete");
  auto* incomplete =
      std::get_if<ais::VDMParseIncomplete>(&result.value());
  TEST_ASSERT_NOT_NULL_MESSAGE(incomplete, "should be VDMParseIncomplete");
}

// ==========================================================================
// Sequential message ID handling
// ==========================================================================

void test_vdm_fields_empty_seq_id() {
  // Single-part messages have empty seq_id field
  auto f = split_fields("1,1,,A,35PLMB5000De@8d<<V6P0Ge60000,0");
  ais::AISReassembler reassembler;

  auto result =
      ais::parse_vdm_fields(f.buffer, f.offsets, f.count, reassembler, 1000);
  TEST_ASSERT_TRUE(result.has_value());

  auto* msg = std::get_if<ais::AISMessage>(&result.value());
  auto* report = std::get_if<ais::ClassAPositionReport>(msg);
  TEST_ASSERT_EQUAL_UINT32(369565000, report->mmsi);
}

// ==========================================================================
// Test runner
// ==========================================================================

int main() {
  UNITY_BEGIN();

  // Single-part
  RUN_TEST(test_vdm_fields_type1_single_part);

  // Multi-part
  RUN_TEST(test_vdm_fields_type5_first_fragment);
  RUN_TEST(test_vdm_fields_type5_complete);

  // Type 18
  RUN_TEST(test_vdm_fields_type18);

  // Error cases
  RUN_TEST(test_vdm_fields_too_few_fields);
  RUN_TEST(test_vdm_fields_invalid_payload);
  RUN_TEST(test_vdm_fields_fragment1_without_fragment2);

  // Seq ID handling
  RUN_TEST(test_vdm_fields_empty_seq_id);

  return UNITY_END();
}
