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

bool connectWifiSta(uint32_t timeoutMs = 12000) {
  if (cfg.wifiSsid.isEmpty()) {
    return false;
  }

  Display::showBootScreen("Connecting Wi-Fi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.wifiSsid.c_str(), cfg.wifiPass.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(250);
  }
  return WiFi.status() == WL_CONNECTED;
}

void startApPortal() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);
  netState.wifiConnected = false;
  netState.apMode = true;
  Display::showPortalScreen(AP_SSID, WiFi.softAPIP().toString().c_str());
}

void setupNetwork() {
  if (connectWifiSta()) {
    netState.wifiConnected = true;
    netState.apMode = false;
    MDNS.begin(MDNS_HOSTNAME);
    applyTimezone();
    Display::showBootScreen("Syncing time...");
  } else {
    startApPortal();
  }
  PortalServer::begin(cfg, weather, flight, netState);
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
  Storage::load(cfg);
  Display::init(cfg.brightness);
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
      }
    }
  }

  unsigned long now = millis();

  // Poll Weather & Flights only when connected and GPS is configured
  if (netState.wifiConnected && cfg.locationConfigured) {
    if (!weatherFetchedOnce || (now - lastWeatherPollMs) >= WEATHER_POLL_INTERVAL_MS) {
      lastWeatherPollMs = now;
      weatherFetchedOnce = true;
      Weather::fetch(cfg, weather);
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
        } else if (flight.active && (now - flight.lastSeenMs) >= FLIGHT_HOLD_MS) {
          flight.active = false;
          digitalWrite(LED_PIN, HIGH);
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
