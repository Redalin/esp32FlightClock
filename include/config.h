
#pragma once

#include <Arduino.h>

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

struct AppConfig {
  // Wi-Fi
  String wifiSsid;
  String wifiPass;

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

  // Flight Radar
  float searchRadiusKm = 5.0f;
  uint16_t flightPollSec = 30;
  int32_t maxAltitudeFt = 5000;
  String flightSource = "adsb.lol"; // "adsb.lol" or "airplanes.live"
};
