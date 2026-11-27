#pragma once

#include <math.h>
#include <stdint.h>

inline float clampWifiTxPowerDbm(float dbm) {
  if (isnan(dbm) || dbm < 5.0f) return 5.0f;
  if (dbm > 20.0f) return 20.0f;
  return roundf(dbm * 2.0f) / 2.0f;
}

inline int8_t clampDisplayOffset(int val) {
  if (val < -30) return -30;
  if (val > 30) return 30;
  return static_cast<int8_t>(val);
}

inline uint16_t normalizeCompassHeading(int val) {
  int m = val % 360;
  if (m < 0) m += 360;
  return static_cast<uint16_t>(m);
}
