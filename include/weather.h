#pragma once

#include "config.h"

struct WeatherData {
  bool valid = false;
  float tempC = 0.0f;
  float feelsLikeC = 0.0f;
  float tempMinC = 0.0f;
  float tempMaxC = 0.0f;
  float windKmh = 0.0f;
  int humidity = 0;
  int weatherCode = 0;
  bool isDay = true;
  unsigned long updatedAtMs = 0;
};

namespace Weather {
  bool fetch(const AppConfig& cfg, WeatherData& out);
  const char* describeCode(int wmoCode);
}
