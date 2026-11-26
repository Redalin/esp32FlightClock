#include "web_server.h"
#include "display.h"
#include "logger.h"
#include "storage.h"
#if __has_include("web_ui_gz.h")
#include "web_ui_gz.h"
#define HAS_WEB_UI_GZ 1
#else
#include "web_ui.h"
#define HAS_WEB_UI_GZ 0
#endif
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <time.h>

namespace {

WebServer server(80);
DNSServer dnsServer;
bool dnsRunning = false;

AppConfig* gCfg = nullptr;
const WeatherData* gWeather = nullptr;
const FlightInfo* gFlight = nullptr;
PortalServer::State* gState = nullptr;

bool parseBool(const String& val) {
  return val == "true" || val == "1" || val == "on";
}

void handleRoot() {
  static constexpr char kUiEtag[] = "\"" FW_VERSION "\"";
  if (server.hasHeader("If-None-Match") && server.header("If-None-Match") == kUiEtag) {
    server.send(304);
    return;
  }
  server.sendHeader("ETag", kUiEtag);
  server.sendHeader("Cache-Control", "public, max-age=3600");
#if HAS_WEB_UI_GZ
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200,
                "text/html; charset=utf-8",
                reinterpret_cast<const char*>(WEB_UI_HTML_GZ),
                WEB_UI_HTML_GZ_LEN);
#else
  server.send_P(200, "text/html; charset=utf-8", WEB_UI_HTML);
#endif
}

void handleGetConfig() {
  JsonDocument doc;
  doc["wifiSsid"] = gCfg->wifiSsid;
  doc["wifiTxPowerDbm"] = gCfg->wifiTxPowerDbm;
  doc["latitude"] = gCfg->latitude;
  doc["longitude"] = gCfg->longitude;
  doc["locationConfigured"] = gCfg->locationConfigured;
  doc["timezone"] = gCfg->timezone;
  doc["use24Hour"] = gCfg->use24Hour;
  doc["useFahrenheit"] = gCfg->useFahrenheit;
  doc["useImperialDist"] = gCfg->useImperialDist;
  doc["brightness"] = gCfg->brightness;
  doc["displayOffsetX"] = gCfg->displayOffsetX;
  doc["displayOffsetY"] = gCfg->displayOffsetY;
  doc["compassHeadingDeg"] = gCfg->compassHeadingDeg;
  doc["searchRadiusKm"] = gCfg->searchRadiusKm;
  doc["flightPollSec"] = gCfg->flightPollSec;
  doc["maxAltitudeFt"] = gCfg->maxAltitudeFt;
  doc["flightSource"] = gCfg->flightSource;
  doc["quietHoursEnabled"] = gCfg->quietHoursEnabled;
  doc["quietStartHour"] = gCfg->quietStartHour;
  doc["quietEndHour"] = gCfg->quietEndHour;

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handlePostConfig() {
  String newSsid = server.arg("wifiSsid");
  newSsid.trim();
  String newPass = server.arg("wifiPass");

  double lat = server.arg("latitude").toDouble();
  double lon = server.arg("longitude").toDouble();
  if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
    server.send(400, "application/json", "{\"message\":\"Invalid GPS coordinates.\"}");
    return;
  }

  bool ssidChanged = (newSsid != gCfg->wifiSsid);
  bool passChanged = (newPass.length() > 0 && newPass != gCfg->wifiPass);

  if (newSsid.length() > 0) {
    gCfg->wifiSsid = newSsid;
  }
  if (newPass.length() > 0) {
    gCfg->wifiPass = newPass;
  } else if (ssidChanged) {
    gCfg->wifiPass = "";
  }

  if (server.hasArg("wifiTxPowerDbm")) {
    float txPwr = server.arg("wifiTxPowerDbm").toFloat();
    if (txPwr >= 5.0f && txPwr <= 20.0f) {
      gCfg->wifiTxPowerDbm = clampWifiTxPowerDbm(txPwr);
      applyWifiTxPower(gCfg->wifiTxPowerDbm);
    }
  }

  gCfg->latitude = lat;
  gCfg->longitude = lon;
  gCfg->locationConfigured = (server.arg("latitude").length() > 0 && server.arg("longitude").length() > 0);

  String tz = server.arg("timezone");
  tz.trim();
  if (tz.length() > 0) {
    gCfg->timezone = tz;
  }

  if (server.hasArg("use24Hour")) gCfg->use24Hour = parseBool(server.arg("use24Hour"));
  if (server.hasArg("useFahrenheit")) gCfg->useFahrenheit = parseBool(server.arg("useFahrenheit"));
  if (server.hasArg("useImperialDist")) gCfg->useImperialDist = parseBool(server.arg("useImperialDist"));

  int bright = server.arg("brightness").toInt();
  if (bright >= 20 && bright <= 255) {
    gCfg->brightness = static_cast<uint8_t>(bright);
    Display::setBrightness(gCfg->brightness);
  }

  if (server.hasArg("displayOffsetX")) {
    gCfg->displayOffsetX = clampDisplayOffset(server.arg("displayOffsetX").toInt());
  }
  if (server.hasArg("displayOffsetY")) {
    gCfg->displayOffsetY = clampDisplayOffset(server.arg("displayOffsetY").toInt());
  }
  Display::setOffset(gCfg->displayOffsetX, gCfg->displayOffsetY);

  if (server.hasArg("compassHeadingDeg")) {
    gCfg->compassHeadingDeg = normalizeCompassHeading(server.arg("compassHeadingDeg").toInt());
  }

  float rad = server.arg("searchRadiusKm").toFloat();
  if (rad >= 1.0f && rad <= 150.0f) {
    gCfg->searchRadiusKm = rad;
  }

  int pollSec = server.arg("flightPollSec").toInt();
  if (pollSec >= 5 && pollSec <= 120) {
    gCfg->flightPollSec = static_cast<uint16_t>(pollSec);
  }

  long maxAlt = server.arg("maxAltitudeFt").toInt();
  if (maxAlt >= 1000 && maxAlt <= 60000) {
    gCfg->maxAltitudeFt = static_cast<int32_t>(maxAlt);
  }

  String src = server.arg("flightSource");
  if (src == "adsb.fi" || src == "airplanes.live" || src == "adsb.lol") {
    gCfg->flightSource = src;
  }

  if (server.hasArg("quietHoursEnabled")) {
    gCfg->quietHoursEnabled = parseBool(server.arg("quietHoursEnabled"));
  }
  if (server.hasArg("quietStartHour")) {
    int qStart = server.arg("quietStartHour").toInt();
    if (qStart >= 0 && qStart <= 23) {
      gCfg->quietStartHour = static_cast<uint8_t>(qStart);
    }
  }
  if (server.hasArg("quietEndHour")) {
    int qEnd = server.arg("quietEndHour").toInt();
    if (qEnd >= 0 && qEnd <= 23) {
      gCfg->quietEndHour = static_cast<uint8_t>(qEnd);
    }
  }

  Storage::save(*gCfg);
  gState->configUpdated = true;
  if ((ssidChanged || passChanged) && gCfg->wifiSsid.length() > 0) {
    gState->wifiReconnectNeeded = true;
  }

  String msg = gState->wifiReconnectNeeded
                   ? "Saved! Connecting to Wi-Fi..."
                   : "Settings saved and applied.";
  server.send(200, "application/json", "{\"message\":\"" + msg + "\"}");
}

void handleStatus() {
  JsonDocument doc;
  doc["wifiConnected"] = gState->wifiConnected;
  doc["ssid"] = gCfg->wifiSsid;
  doc["ip"] = gState->wifiConnected ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  doc["rssi"] = gState->wifiConnected ? WiFi.RSSI() : 0;

  struct tm timeinfo;
  if (gState->timeSynced && getLocalTime(&timeinfo, 10)) {
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    doc["time"] = buf;
  } else {
    doc["time"] = "Not synced";
  }

  if (gWeather->valid) {
    float t = gCfg->useFahrenheit ? (gWeather->tempC * 9.0f / 5.0f + 32.0f) : gWeather->tempC;
    char wBuf[48];
    snprintf(wBuf, sizeof(wBuf), "%.1f %s (%s)",
             t,
             gCfg->useFahrenheit ? "F" : "C",
             Weather::describeCode(gWeather->weatherCode));
    doc["weather"] = wBuf;
  } else {
    doc["weather"] = gCfg->locationConfigured ? "Pending..." : "GPS not set";
  }

  if (gFlight->active) {
    char fBuf[64];
    snprintf(fBuf, sizeof(fBuf), "%s (%s) @ %ld ft, %.1f km",
             gFlight->callsign,
             gFlight->aircraftType[0] ? gFlight->aircraftType : "?",
             static_cast<long>(gFlight->altitudeFt),
             gFlight->distanceKm);
    doc["flight"] = fBuf;
  } else {
    doc["flight"] = "None in range";
  }

  doc["version"] = FW_VERSION;
  doc["firmware"] = "v" FW_VERSION " (" __DATE__ " " __TIME__ ")";

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleScan() {
  if (gState && gState->apMode && WiFi.getMode() == WIFI_AP) {
    WiFi.enableSTA(true);
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false);
    delay(50);
  }

  int n = WiFi.scanNetworks(false, false, false, 120);
  if (gState && gState->apMode) {
    applyWifiTxPower(gCfg->wifiTxPowerDbm);
  }

  struct NetEntry {
    String ssid;
    int32_t rssi;
  };
  NetEntry entries[20];
  int count = 0;

  for (int i = 0; i < n; ++i) {
    String ssid = WiFi.SSID(i);
    if (ssid.isEmpty()) continue;
    int32_t rssi = WiFi.RSSI(i);

    int existing = -1;
    for (int j = 0; j < count; ++j) {
      if (entries[j].ssid == ssid) {
        existing = j;
        break;
      }
    }
    if (existing >= 0) {
      if (rssi > entries[existing].rssi) {
        entries[existing].rssi = rssi;
      }
    } else if (count < 20) {
      entries[count].ssid = ssid;
      entries[count].rssi = rssi;
      ++count;
    }
  }
  WiFi.scanDelete();

  // Sort by strongest RSSI first
  for (int i = 0; i < count; ++i) {
    for (int j = i + 1; j < count; ++j) {
      if (entries[j].rssi > entries[i].rssi) {
        NetEntry tmp = entries[i];
        entries[i] = entries[j];
        entries[j] = tmp;
      }
    }
  }

  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < count; ++i) {
    JsonObject obj = arr.add<JsonObject>();
    obj["ssid"] = entries[i].ssid;
    obj["rssi"] = entries[i].rssi;
  }

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleTestFlight() {
  gState->testFlightRequested = true;
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleDisplayOffset() {
  int8_t ox = server.hasArg("x") ? clampDisplayOffset(server.arg("x").toInt()) : gCfg->displayOffsetX;
  int8_t oy = server.hasArg("y") ? clampDisplayOffset(server.arg("y").toInt()) : gCfg->displayOffsetY;
  Display::setOffset(ox, oy);
  char buf[48];
  snprintf(buf, sizeof(buf), "{\"ok\":true,\"x\":%d,\"y\":%d}", static_cast<int>(ox), static_cast<int>(oy));
  server.send(200, "application/json", buf);
}

void handleReboot() {
  gState->rebootRequested = true;
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleGetLogs() {
  uint32_t since = 0;
  if (server.hasArg("since")) {
    since = static_cast<uint32_t>(strtoul(server.arg("since").c_str(), nullptr, 10));
  }
  String text;
  bool truncated = false;
  uint32_t cursor = Log::readSince(since, text, truncated);

  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Log-Cursor", String(cursor));
  if (truncated) {
    server.sendHeader("X-Log-Truncated", "1");
  }
  if (text.isEmpty() && !truncated) {
    server.send(204, "text/plain; charset=utf-8", "");
  } else {
    server.send(200, "text/plain; charset=utf-8", text);
  }
}

void handleClearLogs() {
  Log::clear();
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", "{\"ok\":true}");
}

bool otaUploadOk = false;
String otaErrorMsg;
size_t otaExpectedSize = 0;
size_t otaWritten = 0;
uint8_t otaLastPct = 255;

void handleOtaUpload() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    if (gState) gState->otaInProgress = true;
    otaUploadOk = false;
    otaErrorMsg = "";
    otaWritten = 0;
    otaLastPct = 255;
    otaExpectedSize = static_cast<size_t>(server.arg("size").toInt());

    Log::printf("[WebOTA] Start: %s (expected=%u bytes)\n",
                upload.filename.c_str(),
                static_cast<unsigned>(otaExpectedSize));
    digitalWrite(LED_PIN, LOW);
    Display::showOtaScreen(0, "Web OTA...");

    size_t updateSize = (otaExpectedSize > 0) ? otaExpectedSize : UPDATE_SIZE_UNKNOWN;
    if (!Update.begin(updateSize, U_FLASH)) {
      otaErrorMsg = Update.errorString();
      Log::printf("[WebOTA] begin failed: %s\n", otaErrorMsg.c_str());
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (otaErrorMsg.isEmpty()) {
      size_t written = Update.write(upload.buf, upload.currentSize);
      if (written != upload.currentSize) {
        otaErrorMsg = Update.errorString();
        Log::printf("[WebOTA] write failed: %s\n", otaErrorMsg.c_str());
      } else {
        otaWritten += written;
        if (otaExpectedSize > 0) {
          uint8_t pct = static_cast<uint8_t>((otaWritten * 100ULL) / otaExpectedSize);
          if (pct > 100) pct = 100;
          if (pct != otaLastPct && (pct == 100 || (pct % 5) == 0)) {
            otaLastPct = pct;
            Display::showOtaScreen(pct, "Flashing...");
          }
        } else if ((otaWritten / 65536U) != otaLastPct) {
          otaLastPct = static_cast<uint8_t>(otaWritten / 65536U);
          Display::showOtaScreen(50, "Flashing...");
        }
      }
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (otaErrorMsg.isEmpty()) {
      if (Update.end(true)) {
        otaUploadOk = true;
        Log::printf("[WebOTA] Success (%u bytes). Rebooting...\n",
                    static_cast<unsigned>(otaWritten));
        Display::showOtaScreen(100, "Rebooting...");
      } else {
        otaErrorMsg = Update.errorString();
        Log::printf("[WebOTA] end failed: %s\n", otaErrorMsg.c_str());
      }
    }
    if (!otaUploadOk) {
      if (gState) gState->otaInProgress = false;
      digitalWrite(LED_PIN, HIGH);
      Display::showOtaScreen(0, "OTA Failed");
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    if (gState) gState->otaInProgress = false;
    otaUploadOk = false;
    otaErrorMsg = "Upload aborted";
    digitalWrite(LED_PIN, HIGH);
    Log::println("[WebOTA] Upload aborted");
    Display::showOtaScreen(0, "OTA Aborted");
  }
}

void handleOtaDone() {
  if (otaUploadOk) {
    server.sendHeader("Connection", "close");
    server.send(200, "application/json",
                "{\"ok\":true,\"message\":\"Firmware updated! Rebooting ESP32...\"}");
    gState->rebootRequested = true;
  } else {
    JsonDocument doc;
    doc["ok"] = false;
    doc["message"] = otaErrorMsg.length() ? ("Update failed: " + otaErrorMsg) : "Update failed";
    String out;
    serializeJson(doc, out);
    server.sendHeader("Connection", "close");
    server.send(500, "application/json", out);
  }
}

void handleNotFound() {
  if (gState->apMode) {
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    server.send(302, "text/plain", "");
    return;
  }
  server.send(404, "text/plain", "Not found");
}

} // namespace

namespace PortalServer {

void begin(AppConfig& cfg,
           const WeatherData& weather,
           const FlightInfo& flight,
           State& state) {
  gCfg = &cfg;
  gWeather = &weather;
  gFlight = &flight;
  gState = &state;

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_POST, handlePostConfig);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/scan", HTTP_GET, handleScan);
  server.on("/api/test-flight", HTTP_POST, handleTestFlight);
  server.on("/api/display-offset", HTTP_POST, handleDisplayOffset);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.on("/api/logs", HTTP_GET, handleGetLogs);
  server.on("/api/logs/clear", HTTP_POST, handleClearLogs);
  server.on("/api/ota", HTTP_POST, handleOtaDone, handleOtaUpload);
  server.onNotFound(handleNotFound);

  static const char* kHeaderKeys[] = {"If-None-Match"};
  server.collectHeaders(kHeaderKeys, 1);

  if (state.apMode && !dnsRunning) {
    dnsServer.start(53, "*", WiFi.softAPIP());
    dnsRunning = true;
  } else if (!state.apMode && dnsRunning) {
    dnsServer.stop();
    dnsRunning = false;
  }

  server.begin();
}

void loop() {
  if (dnsRunning) {
    dnsServer.processNextRequest();
  }
  server.handleClient();
}

} // namespace PortalServer
