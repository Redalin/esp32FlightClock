#pragma once

#include "config.h"
#include "flights.h"
#include "weather.h"

namespace Display {
  void init(uint8_t brightness);
  void setBrightness(uint8_t brightness);
  void showBootScreen(const char* statusLine);
  void showPortalScreen(const char* apSsid, const char* apIp);
  void showOtaScreen(uint8_t percent, const char* statusLine);
  void renderClockWeather(const AppConfig& cfg,
                          const WeatherData& weather,
                          bool wifiOk,
                          bool timeSynced,
                          const char* ipStr);
  void renderFlightScreen(const AppConfig& cfg, const FlightInfo& flight);
}
