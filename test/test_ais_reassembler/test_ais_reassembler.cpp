#include <unity.h>

#include <cstring>

// Include implementation directly for native test linking
#include "ais/ais_reassembler.cpp"

void setUp() {}
void tearDown() {}

// ==========================================================================
// Single-part messages
// ==========================================================================

void test_single_part_returns_immediately() {
  ais::AISReassembler reassembler;
  auto* result = reassembler.add_fragment(
      1, 1, 0, "177KQJ5000G?tO`K>RA1wUbN0TKH", 0, 1000);
  TEST_ASSERT_NOT_NULL_MESSAGE(result, "single-part should return");
  TEST_ASSERT_EQUAL_STRING("177KQJ5000G?tO`K>RA1wUbN0TKH",
                           result->payload);
  TEST_ASSERT_EQUAL_INT(0, result->fill_bits);
}

// ==========================================================================
// Multi-part messages
// ==========================================================================

void test_two_part_message() {
  ais::AISReassembler reassembler;

  // Fragment 1 of 2
  auto* result1 = reassembler.add_fragment(
      2, 1, 0,
      "552HKUh009c=MA?SO38E@P4r080000000000000l00C", 0, 1000);
  TEST_ASSERT_NULL_MESSAGE(result1, "first fragment should not complete");

  // Fragment 2 of 2
  auto* result2 = reassembler.add_fragment(
      2, 2, 0,
      "086s@052iE0j2BhC`0Bh00000000", 2, 1001);
  TEST_ASSERT_NOT_NULL_MESSAGE(result2, "second fragment should complete");

  const char* expected =
      "552HKUh009c=MA?SO38E@P4r080000000000000l00C"
      "086s@052iE0j2BhC`0Bh00000000";
  TEST_ASSERT_EQUAL_STRING(expected, result2->payload);
  TEST_ASSERT_EQUAL_INT(2, result2->fill_bits);
}

void test_reassembler_resets_after_complete() {
  ais::AISReassembler reassembler;

  // Complete a 2-part message
  reassembler.add_fragment(2, 1, 0, "AAAA", 0, 1000);
  reassembler.add_fragment(2, 2, 0, "BBBB", 2, 1001);

  // Start a new single-part message — should work fine
  auto* result = reassembler.add_fragment(1, 1, 0, "CCCC", 0, 2000);
  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_STRING("CCCC", result->payload);
}

// ==========================================================================
// Timeout
// ==========================================================================

void test_timeout_discards_incomplete() {
  ais::AISReassembler reassembler(2000);  // 2 second timeout

  // Fragment 1 of 2
  reassembler.add_fragment(2, 1, 0, "AAAA", 0, 1000);

  // Fragment 2 arrives after timeout (3001 - 1000 = 2001 > 2000)
  auto* result = reassembler.add_fragment(2, 2, 0, "BBBB", 2, 3001);
  TEST_ASSERT_NULL_MESSAGE(result, "timed-out fragment should not complete");
}

void test_new_message_after_timeout() {
  ais::AISReassembler reassembler(2000);

  // Start a 2-part message
  reassembler.add_fragment(2, 1, 0, "AAAA", 0, 1000);

  // Timeout, then start a new single-part message
  auto* result = reassembler.add_fragment(1, 1, 0, "CCCC", 0, 5000);
  TEST_ASSERT_NOT_NULL_MESSAGE(result, "new message after timeout should work");
  TEST_ASSERT_EQUAL_STRING("CCCC", result->payload);
}

// ==========================================================================
// Error cases
// ==========================================================================

void test_out_of_order_fragment_resets() {
  ais::AISReassembler reassembler;

  // Fragment 1 of 3
  reassembler.add_fragment(3, 1, 0, "AAAA", 0, 1000);

  // Fragment 3 arrives (skipping 2) — should reset
  auto* result = reassembler.add_fragment(3, 3, 0, "CCCC", 2, 1001);
  TEST_ASSERT_NULL_MESSAGE(result, "out-of-order should reset");
}

void test_wrong_seq_id_resets() {
  ais::AISReassembler reassembler;

  // Fragment 1 of 2, seq_id = 5
  reassembler.add_fragment(2, 1, 5, "AAAA", 0, 1000);

  // Fragment 2, but seq_id = 6 (different message)
  auto* result = reassembler.add_fragment(2, 2, 6, "BBBB", 2, 1001);
  TEST_ASSERT_NULL_MESSAGE(result, "wrong seq_id should reset");
}

void test_fragment2_without_fragment1() {
  ais::AISReassembler reassembler;

  // Fragment 2 without any prior fragment 1
  auto* result = reassembler.add_fragment(2, 2, 0, "BBBB", 2, 1000);
  TEST_ASSERT_NULL_MESSAGE(result, "fragment 2 without 1 should fail");
}

// ==========================================================================
// Test runner
// ==========================================================================

int main() {
  UNITY_BEGIN();

  // Single-part
  RUN_TEST(test_single_part_returns_immediately);

  // Multi-part
  RUN_TEST(test_two_part_message);
  RUN_TEST(test_reassembler_resets_after_complete);

  // Timeout
  RUN_TEST(test_timeout_discards_incomplete);
  RUN_TEST(test_new_message_after_timeout);

  // Error cases
  RUN_TEST(test_out_of_order_fragment_resets);
  RUN_TEST(test_wrong_seq_id_resets);
  RUN_TEST(test_fragment2_without_fragment1);

  return UNITY_END();
}
