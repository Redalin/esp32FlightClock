
#pragma once

#include <Arduino.h>
#include <esp_wifi.h>
#include <math.h>

// Firmware release version (bump for every release; also appended to the compiled .bin filename)
#define FW_VERSION "1.0.5"

// ESP32-C3 Super Mini onboard LED (active LOW)
#ifndef LED_PIN
#define LED_PIN 8
#endif

// 1.28" 240x240 GC9A01 SPI TFT pins (ESP32-C3 Super Mini)
#define TFT_MOSI_PIN 6
#define TFT_SCLK_PIN 4
#define TFT_CS_PIN   7
#define TFT_DC_PIN   3
#define TFT_RST_PIN  1
#define BACKLIGHT_PIN 0

#define AP_SSID "ESP32-FlightClock"
#define MDNS_HOSTNAME "flightclock"
#define CONFIG_NAMESPACE "flightclock"

// Polling intervals
#define WEATHER_POLL_INTERVAL_MS (10UL * 60UL * 1000UL) // 10 minutes
#define FLIGHT_ACTIVE_POLL_MS    (5UL * 1000UL)        // Poll every 5s while tracking a flight
#define FLIGHT_HOLD_MS           (15UL * 1000UL)       // Keep flight screen 15s after last seen
#define QUIET_SLEEP_GRACE_MS     (5UL * 60UL * 1000UL) // 5 minutes after reboot before deep sleep
#define WIFI_DISCONNECT_RESET_MS (5UL * 60UL * 1000UL) // 5 minutes disconnected (with no AP clients) before reboot

struct AppConfig {
  // Wi-Fi
  String wifiSsid;
  String wifiPass;
  float wifiTxPowerDbm = 5.0f; // 5.0..20.0 dBm in 0.5 dBm steps

  // GPS Location
  double latitude = 0.0;
  double longitude = 0.0;
  bool locationConfigured = false;

  // Time & Display
  String timezone = "UTC0";
  bool use24Hour = true;
  bool useFahrenheit = false;
  bool useImperialDist = false; // false = km & ft/m, true = mi & ft
  uint8_t brightness = 220;     // 20..255
  int8_t displayOffsetX = 0;    // -30..30 px
  int8_t displayOffsetY = 0;    // -30..30 px

  // Flight Radar
  float searchRadiusKm = 5.0f;
  uint16_t flightPollSec = 30;
  int32_t maxAltitudeFt = 5000;
  String flightSource = "airplanes.live"; // "airplanes.live" or "adsb.lol"
  bool quietHoursEnabled = true;
  uint8_t quietStartHour = 1;       // 0..23 (local time)
  uint8_t quietEndHour = 6;         // 0..23 (local time)
};

inline float clampWifiTxPowerDbm(float dbm) {
  if (isnan(dbm) || dbm < 5.0f) return 5.0f;
  if (dbm > 20.0f) return 20.0f;
  return roundf(dbm * 2.0f) / 2.0f;
}

inline void applyWifiTxPower(float dbm) {
  float clamped = clampWifiTxPowerDbm(dbm);
  int8_t quarterDbm = static_cast<int8_t>(lroundf(clamped * 4.0f));
  esp_wifi_set_max_tx_power(quarterDbm);
}

inline int8_t clampDisplayOffset(int val) {
  if (val < -30) return -30;
  if (val > 30) return 30;
  return static_cast<int8_t>(val);
}
