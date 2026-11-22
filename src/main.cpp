#include <Arduino.h>
#include <ArduinoOTA.h>
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
bool otaInitialized = false;
bool otaInProgress = false;

void setupOta() {
  if (!otaInitialized) {
    otaInitialized = true;
    ArduinoOTA.setHostname(MDNS_HOSTNAME);
    ArduinoOTA.setMdnsEnabled(false); // mDNS is managed in setupNetwork()
#ifdef OTA_PASSWORD
    ArduinoOTA.setPassword(OTA_PASSWORD);
#endif
    ArduinoOTA.onStart([]() {
      otaInProgress = true;
      digitalWrite(LED_PIN, LOW);
      Serial.println("[OTA] Start updating firmware");
      Display::showOtaScreen(0, "Starting OTA...");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      static uint8_t lastShownPct = 255;
      uint8_t pct = (total > 0) ? static_cast<uint8_t>((progress * 100U) / total) : 0;
      if (pct != lastShownPct && (pct == 0 || pct == 100 || (pct % 2) == 0)) {
        lastShownPct = pct;
        Display::showOtaScreen(pct, "Flashing...");
      }
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("\n[OTA] Complete, rebooting...");
      Display::showOtaScreen(100, "Rebooting...");
      delay(300);
    });
    ArduinoOTA.onError([](ota_error_t error) {
      otaInProgress = false;
      digitalWrite(LED_PIN, HIGH);
      const char* errText = "OTA Error";
      if (error == OTA_AUTH_ERROR) errText = "Auth Failed";
      else if (error == OTA_BEGIN_ERROR) errText = "Begin Failed";
      else if (error == OTA_CONNECT_ERROR) errText = "Connect Failed";
      else if (error == OTA_RECEIVE_ERROR) errText = "Receive Failed";
      else if (error == OTA_END_ERROR) errText = "End Failed";
      Serial.printf("[OTA] Error[%u]: %s\n", static_cast<unsigned>(error), errText);
      Display::showOtaScreen(0, errText);
      delay(1500);
      lastRenderMs = 0;
    });
  }
  ArduinoOTA.begin();
  Serial.printf("[OTA] Ready on %s.local:3232\n", MDNS_HOSTNAME);
}

void applyTimezone() {
  configTzTime(cfg.timezone.c_str(), "pool.ntp.org", "time.nist.gov");
}

bool isQuietHours(const AppConfig& c, bool timeSynced) {
  if (!c.quietHoursEnabled || !timeSynced) {
    return false;
  }
  if (c.quietStartHour == c.quietEndHour) {
    return false;
  }
  struct tm ti;
  if (!getLocalTime(&ti, 5)) {
    return false;
  }
  int h = ti.tm_hour;
  if (c.quietStartHour < c.quietEndHour) {
    return (h >= c.quietStartHour && h < c.quietEndHour);
  }
  return (h >= c.quietStartHour || h < c.quietEndHour);
}

void registerWifiEvents() {
  static bool eventRegistered = false;
  if (eventRegistered) {
    return;
  }
  eventRegistered = true;
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
      case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        Serial.printf("\n[WiFi] STA disconnected reason=%d\n",
                      info.wifi_sta_disconnected.reason);
        break;
      case ARDUINO_EVENT_WIFI_AP_START:
        Serial.println("[WiFi] AP_START");
        break;
      case ARDUINO_EVENT_WIFI_AP_STOP:
        Serial.println("[WiFi] AP_STOP");
        break;
      case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
        Serial.println("[WiFi] AP client connected");
        break;
      case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
        Serial.printf("[WiFi] AP client IP: %s\n",
                      IPAddress(info.wifi_ap_staipassigned.ip.addr).toString().c_str());
        break;
      case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
        Serial.println("[WiFi] AP client disconnected");
        break;
      default:
        break;
    }
  });
}

bool connectWifiSta(uint32_t timeoutMs = 15000) {
  if (cfg.wifiSsid.isEmpty()) {
    Serial.println("[WiFi] No SSID configured.");
    return false;
  }

  registerWifiEvents();

  Serial.printf("[WiFi] Joining \"%s\" (passLen=%u) ",
                cfg.wifiSsid.c_str(),
                static_cast<unsigned>(cfg.wifiPass.length()));
  Display::showBootScreen("Connecting Wi-Fi...");
  WiFi.persistent(false);
  WiFi.setHostname(MDNS_HOSTNAME); // Must be set before WiFi.mode(WIFI_STA) on ESP32 for DHCP Option 12
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(MDNS_HOSTNAME);
  WiFi.setSleep(false); // Keep RX active for reliable mDNS multicast
  applyWifiTxPower(cfg.wifiTxPowerDbm);
  WiFi.begin(cfg.wifiSsid.c_str(), cfg.wifiPass.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] Connected! IP: %s  Host: %s  RSSI: %d dBm\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.getHostname(),
                  WiFi.RSSI());
    return true;
  }

  Serial.printf("[WiFi] Connection failed (status=%d). Scanning 2.4GHz...\n",
                static_cast<int>(WiFi.status()));
  WiFi.disconnect(false);
  delay(100);
  int n = WiFi.scanNetworks();
  Serial.printf("[WiFi] Scan found %d networks\n", n);
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
  registerWifiEvents();
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(200);

  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1),
                    IPAddress(192, 168, 4, 1),
                    IPAddress(255, 255, 255, 0));
  bool apOk = WiFi.softAP(AP_SSID, nullptr, 1, 0, 4);
  applyWifiTxPower(cfg.wifiTxPowerDbm);
  netState.wifiConnected = false;
  netState.apMode = true;
  Serial.printf("[WiFi] SoftAP \"%s\" %s at http://%s (txPower=%d)\n",
                AP_SSID,
                apOk ? "started" : "FAILED",
                WiFi.softAPIP().toString().c_str(),
                static_cast<int>(WiFi.getTxPower()));
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
#ifdef OTA_PASSWORD
      MDNS.enableArduino(3232, true);
#else
      MDNS.enableArduino(3232, false);
#endif
      Serial.printf("[mDNS] http://%s.local (HTTP + OTA registered)\n", MDNS_HOSTNAME);
    } else {
      Serial.println("[mDNS] Failed to start responder");
    }
    setupOta();
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
  strlcpy(testFlight.origin, "LAX", sizeof(testFlight.origin));
  strlcpy(testFlight.destination, "AKL", sizeof(testFlight.destination));
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
  Serial.println("\n=== ESP32-C3 Flight Clock v" FW_VERSION " ===");

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
  if (netState.wifiConnected) {
    ArduinoOTA.handle();
    if (otaInProgress) {
      delay(2);
      return;
    }
  }

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
        lastRenderMs = 0;
        char tBuf[24];
        strftime(tBuf, sizeof(tBuf), "%Y-%m-%d %H:%M:%S", &ti);
        Serial.printf("[NTP] Time synced: %s\n", tBuf);
      }
    }
  }

  unsigned long now = millis();

  // Render screen every 1s (or immediately when invalidated) before blocking network polls
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
        if (!flight.active && testFlightUntilMs <= now) {
          String ip = netState.wifiConnected ? WiFi.localIP().toString() : "";
          Display::renderClockWeather(cfg, weather, netState.wifiConnected, netState.timeSynced, ip.c_str());
        }
      } else {
        Serial.println("[Weather] Fetch failed");
      }
      lastRenderMs = 0;
    }

    unsigned long flightIntervalMs = flight.active
                                         ? FLIGHT_ACTIVE_POLL_MS
                                         : (static_cast<unsigned long>(cfg.flightPollSec) * 1000UL);
    if (isQuietHours(cfg, netState.timeSynced)) {
      if (flight.active && (now - flight.lastSeenMs) >= FLIGHT_HOLD_MS) {
        flight.active = false;
        digitalWrite(LED_PIN, HIGH);
        lastRenderMs = 0;
        Serial.println("[Flight] Quiet hours active, returning to clock");
      }
    } else if (lastFlightPollMs == 0 || (now - lastFlightPollMs) >= flightIntervalMs) {
      lastFlightPollMs = now;
      FlightInfo latest;
      if (Flights::fetchClosest(cfg, latest)) {
        if (latest.active) {
          flight = latest;
          digitalWrite(LED_PIN, LOW); // Indicate overhead plane
          Serial.printf("[Flight] %s (%s) %s->%s alt=%ldft dist=%.1fkm\n",
                        flight.callsign,
                        flight.aircraftType,
                        flight.origin[0] ? flight.origin : "?",
                        flight.destination[0] ? flight.destination : "?",
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

  delay(10);
}
