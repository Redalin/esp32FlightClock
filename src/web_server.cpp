#include "web_server.h"
#include "display.h"
#include "storage.h"
#include "web_ui.h"
#include <ArduinoJson.h>
#include <DNSServer.h>
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
  server.send_P(200, "text/html; charset=utf-8", WEB_UI_HTML);
}

void handleGetConfig() {
  JsonDocument doc;
  doc["wifiSsid"] = gCfg->wifiSsid;
  doc["latitude"] = gCfg->latitude;
  doc["longitude"] = gCfg->longitude;
  doc["locationConfigured"] = gCfg->locationConfigured;
  doc["timezone"] = gCfg->timezone;
  doc["use24Hour"] = gCfg->use24Hour;
  doc["useFahrenheit"] = gCfg->useFahrenheit;
  doc["useImperialDist"] = gCfg->useImperialDist;
  doc["brightness"] = gCfg->brightness;
  doc["searchRadiusKm"] = gCfg->searchRadiusKm;
  doc["flightPollSec"] = gCfg->flightPollSec;
  doc["maxAltitudeFt"] = gCfg->maxAltitudeFt;
  doc["flightSource"] = gCfg->flightSource;

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
  if (src == "airplanes.live" || src == "adsb.lol") {
    gCfg->flightSource = src;
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

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleScan() {
  int n = WiFi.scanNetworks();
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < n && i < 20; ++i) {
    JsonObject obj = arr.add<JsonObject>();
    obj["ssid"] = WiFi.SSID(i);
    obj["rssi"] = WiFi.RSSI(i);
  }
  WiFi.scanDelete();

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleTestFlight() {
  gState->testFlightRequested = true;
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleReboot() {
  gState->rebootRequested = true;
  server.send(200, "application/json", "{\"ok\":true}");
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
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.onNotFound(handleNotFound);

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
