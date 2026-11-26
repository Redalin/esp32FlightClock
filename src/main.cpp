#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <time.h>

#include "config.h"
#include "display.h"
#include "flights.h"
#include "logger.h"
#include "storage.h"
#include "weather.h"
#include "web_server.h"

namespace {

AppConfig cfg;
WeatherData weather;
FlightInfo flight;
FlightInfo testFlight;
PortalServer::State netState;

unsigned long lastRenderMs = 0;
unsigned long testFlightUntilMs = 0;
unsigned long wifiDisconnectedSinceMs = 0;
unsigned long lastReconnectAttemptMs = 0;
bool otaInitialized = false;
bool otaInProgress = false;
bool wokeFromDeepSleep = false;

// FreeRTOS network polling synchronization
SemaphoreHandle_t netMutex = nullptr;
TaskHandle_t netTaskHandle = nullptr;
AppConfig netCfg;
WeatherData netWeatherResult;
FlightInfo netFlightResult;
bool netWeatherUpdated = false;
bool netFlightUpdated = false;
bool netFlightCleared = false;
volatile bool netForceWeather = false;
volatile bool netForceFlight = false;
volatile bool netResetConn = false;
volatile bool netWifiConnected = false;
volatile bool netTimeSynced = false;
volatile bool netOtaActive = false;
volatile bool netSleepRequested = false;
volatile bool netTaskPaused = true;
volatile bool staGotIpEvent = false;

void syncNetConfigSnapshot() {
  if (netMutex && xSemaphoreTake(netMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
    netCfg = cfg;
    xSemaphoreGive(netMutex);
  }
}

void startMdns() {
  MDNS.end();
  if (MDNS.begin(MDNS_HOSTNAME)) {
    MDNS.setInstanceName("ESP32 Flight Clock");
    MDNS.addService("http", "tcp", 80);
#ifdef OTA_PASSWORD
    MDNS.enableArduino(3232, true);
#else
    MDNS.enableArduino(3232, false);
#endif
    Log::printf("[mDNS] http://%s.local (HTTP + OTA registered)\n", MDNS_HOSTNAME);
  } else {
    Log::println("[mDNS] Failed to start responder");
  }
}

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
      netOtaActive = true;
      netResetConn = true;
      digitalWrite(LED_PIN, LOW);
      Log::println("[OTA] Start updating firmware");
      Display::showOtaScreen(0, "Starting OTA...");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      static uint8_t lastShownPct = 255;
      uint8_t pct = (total > 0) ? static_cast<uint8_t>((progress * 100U) / total) : 0;
      if (pct != lastShownPct && (pct == 0 || pct == 100 || (pct % 5) == 0)) {
        lastShownPct = pct;
        Display::showOtaScreen(pct, "Flashing...");
      }
    });
    ArduinoOTA.onEnd([]() {
      Log::println("[OTA] Complete, rebooting...");
      Display::showOtaScreen(100, "Rebooting...");
      delay(300);
    });
    ArduinoOTA.onError([](ota_error_t error) {
      otaInProgress = false;
      netOtaActive = false;
      digitalWrite(LED_PIN, HIGH);
      const char* errText = "OTA Error";
      if (error == OTA_AUTH_ERROR) errText = "Auth Failed";
      else if (error == OTA_BEGIN_ERROR) errText = "Begin Failed";
      else if (error == OTA_CONNECT_ERROR) errText = "Connect Failed";
      else if (error == OTA_RECEIVE_ERROR) errText = "Receive Failed";
      else if (error == OTA_END_ERROR) errText = "End Failed";
      Log::printf("[OTA] Error[%u]: %s\n", static_cast<unsigned>(error), errText);
      Display::showOtaScreen(0, errText);
      delay(1500);
      lastRenderMs = 0;
    });
  }
  ArduinoOTA.begin();
  Log::printf("[OTA] Ready on %s.local:3232\n", MDNS_HOSTNAME);
}

void applyTimezone() {
  configTzTime(cfg.timezone.c_str(), "pool.ntp.org", "time.nist.gov", "time.cloudflare.com");
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

void pauseNetTaskForShutdown(uint32_t maxWaitMs = 2000) {
  netSleepRequested = true;
  netResetConn = true;
  unsigned long start = millis();
  while (!netTaskPaused && (millis() - start) < maxWaitMs) {
    delay(20);
  }
}

void enterQuietDeepSleep() {
  struct tm ti;
  if (!getLocalTime(&ti, 10)) {
    return;
  }

  int nowSec = ti.tm_hour * 3600 + ti.tm_min * 60 + ti.tm_sec;
  int endSec = static_cast<int>(cfg.quietEndHour) * 3600;
  int deltaSec = endSec - nowSec;
  if (deltaSec <= 0) {
    deltaSec += 24 * 3600;
  }
  deltaSec += 5; // Wake cleanly just after quietEndHour:00:00
  if (deltaSec < 15) {
    deltaSec = 15;
  }

  Log::printf("[Sleep] Quiet hours active; display off & deep sleep for %d s (until %02u:00)\n",
              deltaSec,
              static_cast<unsigned>(cfg.quietEndHour));
  Serial.flush();

  pauseNetTaskForShutdown();
  digitalWrite(LED_PIN, HIGH); // Active LOW LED off
  Display::sleep();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  uint64_t sleepUs = static_cast<uint64_t>(deltaSec) * 1000000ULL;
  esp_sleep_enable_timer_wakeup(sleepUs);
  esp_deep_sleep_start();
}

void netPollTask(void*) {
  WeatherData localWeather;
  bool weatherFetchedOnce = false;
  unsigned long lastWeatherAttemptMs = 0;
  unsigned long weatherNextIntervalMs = WEATHER_RETRY_MIN_MS;

  unsigned long lastFlightPollMs = 0;
  bool localFlightActive = false;
  unsigned long localFlightLastSeenMs = 0;

  for (;;) {
    if (netSleepRequested || netOtaActive || !netWifiConnected) {
      if (netResetConn) {
        netResetConn = false;
        Flights::resetConnection();
      }
      netTaskPaused = true;
      vTaskDelay(pdMS_TO_TICKS(250));
      continue;
    }

    netTaskPaused = false;
    if (netResetConn) {
      netResetConn = false;
      Flights::resetConnection();
    }

    AppConfig localCfg;
    if (netMutex && xSemaphoreTake(netMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
      localCfg = netCfg;
      xSemaphoreGive(netMutex);
    } else {
      vTaskDelay(pdMS_TO_TICKS(250));
      continue;
    }

    if (!localCfg.locationConfigured) {
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    unsigned long now = millis();

    if (netForceWeather) {
      netForceWeather = false;
      weatherFetchedOnce = false;
      lastWeatherAttemptMs = 0;
      weatherNextIntervalMs = WEATHER_RETRY_MIN_MS;
    }

    if (lastWeatherAttemptMs == 0 || (now - lastWeatherAttemptMs) >= weatherNextIntervalMs) {
      lastWeatherAttemptMs = now;
      if (Weather::fetch(localCfg, localWeather)) {
        weatherFetchedOnce = true;
        weatherNextIntervalMs = WEATHER_POLL_INTERVAL_MS;
        Log::printf("[Weather] %.1f C, %s (RH %d%%)\n",
                    localWeather.tempC,
                    Weather::describeCode(localWeather.weatherCode),
                    localWeather.humidity);
        if (netMutex && xSemaphoreTake(netMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
          netWeatherResult = localWeather;
          netWeatherUpdated = true;
          xSemaphoreGive(netMutex);
        }
      } else {
        if (weatherFetchedOnce && weatherNextIntervalMs >= WEATHER_POLL_INTERVAL_MS) {
          weatherNextIntervalMs = WEATHER_RETRY_MIN_MS;
        } else {
          weatherNextIntervalMs *= 2UL;
          if (weatherNextIntervalMs > WEATHER_RETRY_MAX_MS) {
            weatherNextIntervalMs = WEATHER_RETRY_MAX_MS;
          }
        }
        Log::printf("[Weather] Fetch failed; retrying in %lus\n", weatherNextIntervalMs / 1000UL);
      }
    }

    if (netSleepRequested || netOtaActive || !netWifiConnected) {
      continue;
    }

    if (netForceFlight) {
      netForceFlight = false;
      lastFlightPollMs = 0;
    }

    now = millis();
    if (isQuietHours(localCfg, netTimeSynced)) {
      if (localFlightActive && (now - localFlightLastSeenMs) >= FLIGHT_HOLD_MS) {
        localFlightActive = false;
        if (netMutex && xSemaphoreTake(netMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
          netFlightCleared = true;
          xSemaphoreGive(netMutex);
        }
        Log::println("[Flight] Quiet hours active, returning to clock");
      }
    } else {
      unsigned long flightIntervalMs = localFlightActive
                                           ? FLIGHT_ACTIVE_POLL_MS
                                           : (static_cast<unsigned long>(localCfg.flightPollSec) * 1000UL);
      if (lastFlightPollMs == 0 || (now - lastFlightPollMs) >= flightIntervalMs) {
        lastFlightPollMs = now;
        FlightInfo latest;
        if (Flights::fetchClosest(localCfg, latest)) {
          if (latest.active) {
            localFlightActive = true;
            localFlightLastSeenMs = latest.lastSeenMs;
            Log::printf("[Flight] %s (%s) %s->%s alt=%ldft dist=%.1fkm\n",
                        latest.callsign,
                        latest.aircraftType,
                        latest.origin[0] ? latest.origin : "?",
                        latest.destination[0] ? latest.destination : "?",
                        static_cast<long>(latest.altitudeFt),
                        latest.distanceKm);
            if (netMutex && xSemaphoreTake(netMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
              netFlightResult = latest;
              netFlightUpdated = true;
              netFlightCleared = false;
              xSemaphoreGive(netMutex);
            }
          } else if (localFlightActive && (now - localFlightLastSeenMs) >= FLIGHT_HOLD_MS) {
            localFlightActive = false;
            if (netMutex && xSemaphoreTake(netMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
              netFlightCleared = true;
              xSemaphoreGive(netMutex);
            }
            Log::println("[Flight] Cleared, returning to clock");
          }
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

void registerWifiEvents() {
  static bool eventRegistered = false;
  if (eventRegistered) {
    return;
  }
  eventRegistered = true;
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
      case ARDUINO_EVENT_WIFI_STA_START:
      case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        applyWifiTxPower(cfg.wifiTxPowerDbm);
        break;
      case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        applyWifiTxPower(cfg.wifiTxPowerDbm);
        staGotIpEvent = true;
        break;
      case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        Log::printf("[WiFi] STA disconnected reason=%d\n",
                    info.wifi_sta_disconnected.reason);
        netResetConn = true;
        break;
      case ARDUINO_EVENT_WIFI_AP_START:
        Log::println("[WiFi] AP_START");
        break;
      case ARDUINO_EVENT_WIFI_AP_STOP:
        Log::println("[WiFi] AP_STOP");
        break;
      case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
        Log::println("[WiFi] AP client connected");
        break;
      case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
        Log::printf("[WiFi] AP client IP: %s\n",
                    IPAddress(info.wifi_ap_staipassigned.ip.addr).toString().c_str());
        break;
      case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
        Log::println("[WiFi] AP client disconnected");
        break;
      default:
        break;
    }
  });
}

bool connectWifiSta(uint32_t timeoutMs = WIFI_CONNECT_TIMEOUT_MS,
                    uint8_t maxAttempts = WIFI_CONNECT_ATTEMPTS) {
  if (cfg.wifiSsid.isEmpty()) {
    Log::println("[WiFi] No SSID configured.");
    return false;
  }

  registerWifiEvents();

  WiFi.persistent(false);
  WiFi.setHostname(MDNS_HOSTNAME); // Must be set before WiFi.mode(WIFI_STA) on ESP32 for DHCP Option 12
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(MDNS_HOSTNAME);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false); // Keep RX active for reliable mDNS multicast & weak-link ACKs
  esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
  applyWifiTxPower(cfg.wifiTxPowerDbm);

  for (uint8_t attempt = 1; attempt <= maxAttempts; ++attempt) {
    char bootMsg[32];
    if (attempt == 1) {
      snprintf(bootMsg, sizeof(bootMsg), "Connecting Wi-Fi...");
    } else {
      snprintf(bootMsg, sizeof(bootMsg), "Wi-Fi retry %u/%u...",
               static_cast<unsigned>(attempt),
               static_cast<unsigned>(maxAttempts));
    }
    Display::showBootScreen(bootMsg);

    Log::printf("[WiFi] Attempt %u/%u joining \"%s\" (passLen=%u)\n",
                static_cast<unsigned>(attempt),
                static_cast<unsigned>(maxAttempts),
                cfg.wifiSsid.c_str(),
                static_cast<unsigned>(cfg.wifiPass.length()));

    if (attempt > 1) {
      WiFi.disconnect(false);
      delay(300);
    }
    WiFi.begin(cfg.wifiSsid.c_str(), cfg.wifiPass.c_str());
    applyWifiTxPower(cfg.wifiTxPowerDbm);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
      delay(250);
      Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      applyWifiTxPower(cfg.wifiTxPowerDbm);
      int8_t actualQuarterDbm = 0;
      esp_wifi_get_max_tx_power(&actualQuarterDbm);
      Log::printf("[WiFi] Connected! IP: %s  Host: %s  RSSI: %d dBm  TxPower: %.2f dBm\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.getHostname(),
                  WiFi.RSSI(),
                  actualQuarterDbm * 0.25f);
      return true;
    }

    Log::printf("[WiFi] Attempt %u failed (status=%d)\n",
                static_cast<unsigned>(attempt),
                static_cast<int>(WiFi.status()));
  }

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
  netWifiConnected = false;
  Log::printf("[WiFi] SoftAP \"%s\" %s at http://%s (txPower=%d)\n",
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
    netWifiConnected = true;
    staGotIpEvent = false;
    startMdns();
    setupOta();
    applyTimezone();
    Log::printf("[NTP] Syncing time (TZ=%s)\n", cfg.timezone.c_str());
    Display::showBootScreen("Syncing time...");
  } else {
    startApPortal();
  }
  PortalServer::begin(cfg, weather, flight, netState);
  Log::println("[Web] Portal server listening on port 80");
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
  Log::init();
  wokeFromDeepSleep = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER);
  Log::println("=== ESP32-C3 Flight Clock v" FW_VERSION " ===");
  if (wokeFromDeepSleep) {
    Log::println("[Sleep] Woke from deep sleep timer");
  }

  Storage::load(cfg);
  Log::printf("[Cfg] SSID=\"%s\" txPwr=%.1fdBm GPS=%s (%.4f, %.4f) TZ=%s offset=(%d,%d)\n",
              cfg.wifiSsid.c_str(),
              cfg.wifiTxPowerDbm,
              cfg.locationConfigured ? "yes" : "no",
              cfg.latitude,
              cfg.longitude,
              cfg.timezone.c_str(),
              static_cast<int>(cfg.displayOffsetX),
              static_cast<int>(cfg.displayOffsetY));

  Display::init(cfg.brightness);
  Display::setOffset(cfg.displayOffsetX, cfg.displayOffsetY);
  Log::println("[Display] GC9A01 initialized");
  Display::showBootScreen("Starting up...");

  netMutex = xSemaphoreCreateMutex();
  syncNetConfigSnapshot();

  setupNetwork();

  xTaskCreate(netPollTask, "netPoll", 8192, nullptr, 1, &netTaskHandle);
}

void loop() {
  bool anyOtaActive = otaInProgress || netState.otaInProgress;
  netOtaActive = anyOtaActive;

  if (netState.wifiConnected) {
    ArduinoOTA.handle();
    if (otaInProgress) {
      delay(2);
      return;
    }
  }

  PortalServer::loop();

  if (netState.rebootRequested) {
    pauseNetTaskForShutdown(500);
    delay(300);
    ESP.restart();
  }

  if (netState.wifiReconnectNeeded) {
    netState.wifiReconnectNeeded = false;
    syncNetConfigSnapshot();
    netWifiConnected = false;
    netResetConn = true;
    WiFi.disconnect(true);
    delay(200);
    setupNetwork();
    netForceWeather = true;
    netForceFlight = true;
  }

  if (netState.configUpdated) {
    netState.configUpdated = false;
    syncNetConfigSnapshot();
    if (netState.wifiConnected) {
      applyTimezone();
    }
    netForceWeather = true;
    netForceFlight = true;
    lastRenderMs = 0;
  }

  if (netState.testFlightRequested) {
    netState.testFlightRequested = false;
    triggerTestFlight();
  }

  unsigned long now = millis();

  // Track Wi-Fi & NTP state
  if (!netState.apMode) {
    bool connectedNow = (WiFi.status() == WL_CONNECTED);
    netState.wifiConnected = connectedNow;
    netWifiConnected = connectedNow;

    if (staGotIpEvent && connectedNow) {
      staGotIpEvent = false;
      applyWifiTxPower(cfg.wifiTxPowerDbm);
      startMdns();
      if (!netState.timeSynced) {
        applyTimezone();
      }
      Log::printf("[WiFi] Link up: %s (RSSI %d dBm)\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.RSSI());
      lastRenderMs = 0;
    }

    if (!connectedNow && !anyOtaActive && !cfg.wifiSsid.isEmpty()) {
      if (lastReconnectAttemptMs == 0 || (now - lastReconnectAttemptMs) >= WIFI_RECONNECT_INTERVAL_MS) {
        lastReconnectAttemptMs = now;
        Log::println("[WiFi] Link down; attempting reconnect...");
        applyWifiTxPower(cfg.wifiTxPowerDbm);
        WiFi.reconnect();
      }
    } else if (connectedNow) {
      lastReconnectAttemptMs = 0;
    }

    if (!netState.timeSynced) {
      struct tm ti;
      if (getLocalTime(&ti, 5) && ti.tm_year > (2020 - 1900)) {
        netState.timeSynced = true;
        netTimeSynced = true;
        lastRenderMs = 0;
        char tBuf[24];
        strftime(tBuf, sizeof(tBuf), "%Y-%m-%d %H:%M:%S", &ti);
        Log::printf("[NTP] Time synced: %s\n", tBuf);
      }
    }
  }

  // Drain results from background network polling task
  if (netMutex && xSemaphoreTake(netMutex, 0) == pdTRUE) {
    if (netWeatherUpdated) {
      weather = netWeatherResult;
      netWeatherUpdated = false;
      lastRenderMs = 0;
    }
    if (netFlightUpdated) {
      flight = netFlightResult;
      netFlightUpdated = false;
      digitalWrite(LED_PIN, LOW); // Indicate overhead plane
      lastRenderMs = 0;
    } else if (netFlightCleared) {
      flight.active = false;
      netFlightCleared = false;
      digitalWrite(LED_PIN, HIGH);
      lastRenderMs = 0;
    }
    xSemaphoreGive(netMutex);
  }

  // Auto-reset if disconnected from Wi-Fi for 5 minutes with no hotspot clients connected
  if (!anyOtaActive && !cfg.wifiSsid.isEmpty()) {
    bool hasApClients = netState.apMode && (WiFi.softAPgetStationNum() > 0);
    if (netState.wifiConnected || hasApClients) {
      wifiDisconnectedSinceMs = 0;
    } else if (wifiDisconnectedSinceMs == 0) {
      wifiDisconnectedSinceMs = now ? now : 1UL;
    } else if ((now - wifiDisconnectedSinceMs) >= WIFI_DISCONNECT_RESET_MS) {
      Log::println("[WiFi] Disconnected for 5 minutes with no hotspot clients; rebooting to retry Wi-Fi...");
      Serial.flush();
      pauseNetTaskForShutdown(500);
      delay(200);
      ESP.restart();
    }
  } else {
    wifiDisconnectedSinceMs = 0;
  }

  // Quiet hours: turn display off and deep sleep after 5-minute post-reboot grace period
  if (!netState.apMode && !anyOtaActive && isQuietHours(cfg, netState.timeSynced)) {
    if (wokeFromDeepSleep || now >= QUIET_SLEEP_GRACE_MS) {
      enterQuietDeepSleep();
    }
  }

  // Render screen every 1s (or immediately when invalidated)
  if (!netState.apMode && !anyOtaActive && (lastRenderMs == 0 || (now - lastRenderMs) >= 1000UL)) {
    lastRenderMs = now;
    if (testFlightUntilMs > now) {
      Display::renderFlightScreen(cfg, testFlight);
    } else if (flight.active) {
      Display::renderFlightScreen(cfg, flight);
    } else {
      char ipBuf[16] = {0};
      if (!cfg.locationConfigured && netState.wifiConnected) {
        IPAddress ip = WiFi.localIP();
        snprintf(ipBuf, sizeof(ipBuf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
      }
      Display::renderClockWeather(cfg, weather, netState.wifiConnected, netState.timeSynced, ipBuf);
    }
  }

  delay(10);
}
