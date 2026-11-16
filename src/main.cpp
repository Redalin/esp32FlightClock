#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <time.h>

#include "config.h"
#include "display.h"
#include "flights.h"
#include "storage.h"
#include "weather.h"
#include "web_server.h"

namespace {

AppConfig cfg;
WeatherData weather;
FlightInfo flight;
FlightInfo testFlight;
PortalServer::State netState;

unsigned long lastWeatherPollMs = 0;
unsigned long lastFlightPollMs = 0;
unsigned long lastRenderMs = 0;
unsigned long testFlightUntilMs = 0;
bool weatherFetchedOnce = false;

void applyTimezone() {
  configTzTime(cfg.timezone.c_str(), "pool.ntp.org", "time.nist.gov");
}

bool connectWifiSta(uint32_t timeoutMs = 15000) {
  if (cfg.wifiSsid.isEmpty()) {
    Serial.println("[WiFi] No SSID configured.");
    return false;
  }

  static bool eventRegistered = false;
  if (!eventRegistered) {
    eventRegistered = true;
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
      if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        Serial.printf("\n[WiFi] Disconnected reason=%d\n",
                      info.wifi_sta_disconnected.reason);
      }
    });
  }

  Serial.printf("[WiFi] Joining \"%s\" (passLen=%u) ",
                cfg.wifiSsid.c_str(),
                static_cast<unsigned>(cfg.wifiPass.length()));
  Display::showBootScreen("Connecting Wi-Fi...");
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(MDNS_HOSTNAME);
  WiFi.setSleep(false); // Keep RX active for reliable mDNS multicast
  WiFi.setTxPower(WIFI_POWER_11dBm); // Prevents RF brownout on ESP32-C3 Super Mini
  WiFi.begin(cfg.wifiSsid.c_str(), cfg.wifiPass.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] Connected! IP: %s  RSSI: %d dBm\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.RSSI());
    return true;
  }

  Serial.printf("[WiFi] Connection failed (status=%d). Scanning 2.4GHz...\n",
                static_cast<int>(WiFi.status()));
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n && i < 8; ++i) {
    Serial.printf("  - \"%s\" ch=%d rssi=%d\n",
                  WiFi.SSID(i).c_str(),
                  WiFi.channel(i),
                  WiFi.RSSI(i));
  }
  WiFi.scanDelete();
  return false;
}

void startApPortal() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);
  netState.wifiConnected = false;
  netState.apMode = true;
  Serial.printf("[WiFi] Started AP \"%s\" at http://%s\n",
                AP_SSID,
                WiFi.softAPIP().toString().c_str());
  Display::showPortalScreen(AP_SSID, WiFi.softAPIP().toString().c_str());
}

void setupNetwork() {
  if (connectWifiSta()) {
    netState.wifiConnected = true;
    netState.apMode = false;
    MDNS.end();
    if (MDNS.begin(MDNS_HOSTNAME)) {
      MDNS.setInstanceName("ESP32 Flight Clock");
      MDNS.addService("http", "tcp", 80);
      Serial.printf("[mDNS] http://%s.local (HTTP service registered)\n", MDNS_HOSTNAME);
    } else {
      Serial.println("[mDNS] Failed to start responder");
    }
    applyTimezone();
    Serial.printf("[NTP] Syncing time (TZ=%s)\n", cfg.timezone.c_str());
    Display::showBootScreen("Syncing time...");
  } else {
    startApPortal();
  }
  PortalServer::begin(cfg, weather, flight, netState);
  Serial.println("[Web] Portal server listening on port 80");
}

void triggerTestFlight() {
  testFlight.active = true;
  strlcpy(testFlight.callsign, "ANZ123", sizeof(testFlight.callsign));
  strlcpy(testFlight.registration, "ZK-NZE", sizeof(testFlight.registration));
  strlcpy(testFlight.aircraftType, "B789", sizeof(testFlight.aircraftType));
  strlcpy(testFlight.squawk, "4210", sizeof(testFlight.squawk));
  testFlight.altitudeFt = 28400;
  testFlight.verticalRateFpm = 1450;
  testFlight.speedKnots = 468.0f;
  testFlight.trackDeg = 142.0f;
  testFlight.distanceKm = 3.8f;
  testFlight.bearingDeg = 55.0f;
  testFlight.totalInRange = 2;
  testFlightUntilMs = millis() + 10000UL;
  lastRenderMs = 0;
}

} // namespace

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // Active LOW LED off

  Serial.begin(115200);
  delay(800); // Allow USB-CDC host to attach
  Serial.println("\n=== ESP32-C3 Flight Clock ===");

  Storage::load(cfg);
  Serial.printf("[Cfg] SSID=\"%s\" GPS=%s (%.4f, %.4f) TZ=%s\n",
                cfg.wifiSsid.c_str(),
                cfg.locationConfigured ? "yes" : "no",
                cfg.latitude,
                cfg.longitude,
                cfg.timezone.c_str());

  Display::init(cfg.brightness);
  Serial.println("[Display] GC9A01 initialized");
  Display::showBootScreen("Starting up...");

  setupNetwork();
}

void loop() {
  PortalServer::loop();

  if (netState.rebootRequested) {
    delay(300);
    ESP.restart();
  }

  if (netState.wifiReconnectNeeded) {
    netState.wifiReconnectNeeded = false;
    WiFi.disconnect(true);
    delay(200);
    setupNetwork();
    weatherFetchedOnce = false;
    lastWeatherPollMs = 0;
    lastFlightPollMs = 0;
  }

  if (netState.configUpdated) {
    netState.configUpdated = false;
    if (netState.wifiConnected) {
      applyTimezone();
    }
    weatherFetchedOnce = false;
    lastWeatherPollMs = 0;
    lastFlightPollMs = 0;
    lastRenderMs = 0;
  }

  if (netState.testFlightRequested) {
    netState.testFlightRequested = false;
    triggerTestFlight();
  }

  // Track Wi-Fi & NTP state
  if (!netState.apMode) {
    netState.wifiConnected = (WiFi.status() == WL_CONNECTED);
    if (!netState.timeSynced) {
      struct tm ti;
      if (getLocalTime(&ti, 5) && ti.tm_year > (2020 - 1900)) {
        netState.timeSynced = true;
        char tBuf[24];
        strftime(tBuf, sizeof(tBuf), "%Y-%m-%d %H:%M:%S", &ti);
        Serial.printf("[NTP] Time synced: %s\n", tBuf);
      }
    }
  }

  unsigned long now = millis();

  // Poll Weather & Flights only when connected and GPS is configured
  if (netState.wifiConnected && cfg.locationConfigured) {
    if (!weatherFetchedOnce || (now - lastWeatherPollMs) >= WEATHER_POLL_INTERVAL_MS) {
      lastWeatherPollMs = now;
      weatherFetchedOnce = true;
      if (Weather::fetch(cfg, weather)) {
        Serial.printf("[Weather] %.1f C, %s (RH %d%%)\n",
                      weather.tempC,
                      Weather::describeCode(weather.weatherCode),
                      weather.humidity);
      } else {
        Serial.println("[Weather] Fetch failed");
      }
      lastRenderMs = 0;
    }

    unsigned long flightIntervalMs = static_cast<unsigned long>(cfg.flightPollSec) * 1000UL;
    if (lastFlightPollMs == 0 || (now - lastFlightPollMs) >= flightIntervalMs) {
      lastFlightPollMs = now;
      FlightInfo latest;
      if (Flights::fetchClosest(cfg, latest)) {
        if (latest.active) {
          flight = latest;
          digitalWrite(LED_PIN, LOW); // Indicate overhead plane
          Serial.printf("[Flight] %s (%s) alt=%ldft dist=%.1fkm\n",
                        flight.callsign,
                        flight.aircraftType,
                        static_cast<long>(flight.altitudeFt),
                        flight.distanceKm);
        } else if (flight.active && (now - flight.lastSeenMs) >= FLIGHT_HOLD_MS) {
          flight.active = false;
          digitalWrite(LED_PIN, HIGH);
          Serial.println("[Flight] Cleared, returning to clock");
        }
        lastRenderMs = 0;
      }
    }
  }

  // Render screen every 1s (or immediately when invalidated)
  if (!netState.apMode && (lastRenderMs == 0 || (now - lastRenderMs) >= 1000UL)) {
    lastRenderMs = now;
    if (testFlightUntilMs > now) {
      Display::renderFlightScreen(cfg, testFlight);
    } else if (flight.active) {
      Display::renderFlightScreen(cfg, flight);
    } else {
      String ip = netState.wifiConnected ? WiFi.localIP().toString() : "";
      Display::renderClockWeather(cfg, weather, netState.wifiConnected, netState.timeSynced, ip.c_str());
    }
  }

  delay(10);
}
