#include <unity.h>

#include <cstring>

#include "operating_mode.h"

using ais_interface::OperatingMode;
using ais_interface::OperatingModeLogic;

void setUp() {}
void tearDown() {}

// ==========================================================================
// Initial state
// ==========================================================================

void test_initial_mode_is_tx_rx() {
  OperatingModeLogic logic;
  TEST_ASSERT_EQUAL(OperatingMode::kTransmitReceive, logic.get_mode());
}

void test_initial_effective_mmsi_is_zeros() {
  OperatingModeLogic logic;
  TEST_ASSERT_EQUAL_STRING("000000000", logic.effective_mmsi());
}

// ==========================================================================
// Mode transitions
// ==========================================================================

void test_switch_to_rx_only_returns_zero_mmsi() {
  OperatingModeLogic logic;
  logic.set_user_mmsi("123456789");

  auto result = logic.set_mode(OperatingMode::kReceiveOnly);
  TEST_ASSERT_TRUE(result.has_value());
  TEST_ASSERT_EQUAL_STRING("000000000", result.value());
}

void test_switch_to_tx_rx_returns_user_mmsi() {
  OperatingModeLogic logic;
  logic.set_user_mmsi("123456789");
  logic.set_mode(OperatingMode::kReceiveOnly);

  auto result = logic.set_mode(OperatingMode::kTransmitReceive);
  TEST_ASSERT_TRUE(result.has_value());
  TEST_ASSERT_EQUAL_STRING("123456789", result.value());
}

void test_same_mode_returns_nullopt() {
  OperatingModeLogic logic;
  logic.set_user_mmsi("123456789");

  // Already in TX+RX
  auto result = logic.set_mode(OperatingMode::kTransmitReceive);
  TEST_ASSERT_FALSE(result.has_value());
}

void test_switch_to_rx_only_twice_returns_nullopt() {
  OperatingModeLogic logic;
  logic.set_user_mmsi("123456789");
  logic.set_mode(OperatingMode::kReceiveOnly);

  auto result = logic.set_mode(OperatingMode::kReceiveOnly);
  TEST_ASSERT_FALSE(result.has_value());
}

// ==========================================================================
// Effective MMSI
// ==========================================================================

void test_effective_mmsi_in_rx_only() {
  OperatingModeLogic logic;
  logic.set_user_mmsi("123456789");
  logic.set_mode(OperatingMode::kReceiveOnly);

  TEST_ASSERT_EQUAL_STRING("000000000", logic.effective_mmsi());
}

void test_effective_mmsi_in_tx_rx() {
  OperatingModeLogic logic;
  logic.set_user_mmsi("123456789");

  TEST_ASSERT_EQUAL_STRING("123456789", logic.effective_mmsi());
}

// ==========================================================================
// User MMSI changes
// ==========================================================================

void test_mmsi_change_in_tx_rx_returns_new_mmsi() {
  OperatingModeLogic logic;
  logic.set_user_mmsi("111111111");

  auto result = logic.on_user_mmsi_changed("222222222");
  TEST_ASSERT_TRUE(result.has_value());
  TEST_ASSERT_EQUAL_STRING("222222222", result.value());
}

void test_mmsi_change_in_rx_only_returns_nullopt() {
  OperatingModeLogic logic;
  logic.set_user_mmsi("111111111");
  logic.set_mode(OperatingMode::kReceiveOnly);

  auto result = logic.on_user_mmsi_changed("222222222");
  TEST_ASSERT_FALSE_MESSAGE(result.has_value(),
                            "should not update transponder in RX-only");
}

void test_mmsi_change_in_rx_only_preserves_user_mmsi() {
  OperatingModeLogic logic;
  logic.set_user_mmsi("111111111");
  logic.set_mode(OperatingMode::kReceiveOnly);

  logic.on_user_mmsi_changed("222222222");
  TEST_ASSERT_EQUAL_STRING("222222222", logic.get_user_mmsi());
}

void test_mmsi_restored_after_rx_only_to_tx_rx() {
  OperatingModeLogic logic;
  logic.set_user_mmsi("111111111");
  logic.set_mode(OperatingMode::kReceiveOnly);

  // Change MMSI while in RX-only
  logic.on_user_mmsi_changed("222222222");

  // Switch back to TX+RX — should use the updated MMSI
  auto result = logic.set_mode(OperatingMode::kTransmitReceive);
  TEST_ASSERT_TRUE(result.has_value());
  TEST_ASSERT_EQUAL_STRING("222222222", result.value());
}

// ==========================================================================
// Test runner
// ==========================================================================

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_initial_mode_is_tx_rx);
  RUN_TEST(test_initial_effective_mmsi_is_zeros);

  RUN_TEST(test_switch_to_rx_only_returns_zero_mmsi);
  RUN_TEST(test_switch_to_tx_rx_returns_user_mmsi);
  RUN_TEST(test_same_mode_returns_nullopt);
  RUN_TEST(test_switch_to_rx_only_twice_returns_nullopt);

  RUN_TEST(test_effective_mmsi_in_rx_only);
  RUN_TEST(test_effective_mmsi_in_tx_rx);

  RUN_TEST(test_mmsi_change_in_tx_rx_returns_new_mmsi);
  RUN_TEST(test_mmsi_change_in_rx_only_returns_nullopt);
  RUN_TEST(test_mmsi_change_in_rx_only_preserves_user_mmsi);
  RUN_TEST(test_mmsi_restored_after_rx_only_to_tx_rx);

  return UNITY_END();
}
