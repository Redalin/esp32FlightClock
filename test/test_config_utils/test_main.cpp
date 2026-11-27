#include <unity.h>
#include <math.h>
#include "config_utils.h"

void setUp(void) {}
void tearDown(void) {}

void test_clamp_wifi_tx_power_bounds(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, clampWifiTxPowerDbm(0.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, clampWifiTxPowerDbm(-10.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, clampWifiTxPowerDbm(NAN));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, clampWifiTxPowerDbm(25.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, clampWifiTxPowerDbm(20.0f));
}

void test_clamp_wifi_tx_power_half_dbm_rounding(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 8.5f, clampWifiTxPowerDbm(8.5f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 8.5f, clampWifiTxPowerDbm(8.4f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 9.0f, clampWifiTxPowerDbm(8.8f));
}

void test_clamp_display_offset(void) {
  TEST_ASSERT_EQUAL_INT8(-30, clampDisplayOffset(-100));
  TEST_ASSERT_EQUAL_INT8(-30, clampDisplayOffset(-30));
  TEST_ASSERT_EQUAL_INT8(0, clampDisplayOffset(0));
  TEST_ASSERT_EQUAL_INT8(15, clampDisplayOffset(15));
  TEST_ASSERT_EQUAL_INT8(30, clampDisplayOffset(30));
  TEST_ASSERT_EQUAL_INT8(30, clampDisplayOffset(99));
}

void test_normalize_compass_heading(void) {
  TEST_ASSERT_EQUAL_UINT16(0, normalizeCompassHeading(0));
  TEST_ASSERT_EQUAL_UINT16(0, normalizeCompassHeading(360));
  TEST_ASSERT_EQUAL_UINT16(90, normalizeCompassHeading(450));
  TEST_ASSERT_EQUAL_UINT16(350, normalizeCompassHeading(-10));
  TEST_ASSERT_EQUAL_UINT16(270, normalizeCompassHeading(-450));
  TEST_ASSERT_EQUAL_UINT16(359, normalizeCompassHeading(359));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_clamp_wifi_tx_power_bounds);
  RUN_TEST(test_clamp_wifi_tx_power_half_dbm_rounding);
  RUN_TEST(test_clamp_display_offset);
  RUN_TEST(test_normalize_compass_heading);
  return UNITY_END();
}
