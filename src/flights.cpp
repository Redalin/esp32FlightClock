#include "flights.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <math.h>

namespace {

constexpr double kDegToRad = 0.01745329251994329577;
constexpr double kRadToDeg = 57.2957795130823208768;
constexpr double kEarthRadiusKm = 6371.0;

float haversineKm(double lat1, double lon1, double lat2, double lon2) {
  double dLat = (lat2 - lat1) * kDegToRad;
  double dLon = (lon2 - lon1) * kDegToRad;
  double a = sin(dLat / 2.0) * sin(dLat / 2.0) +
             cos(lat1 * kDegToRad) * cos(lat2 * kDegToRad) *
                 sin(dLon / 2.0) * sin(dLon / 2.0);
  double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
  return static_cast<float>(kEarthRadiusKm * c);
}

float initialBearingDeg(double lat1, double lon1, double lat2, double lon2) {
  double phi1 = lat1 * kDegToRad;
  double phi2 = lat2 * kDegToRad;
  double dLon = (lon2 - lon1) * kDegToRad;
  double y = sin(dLon) * cos(phi2);
  double x = cos(phi1) * sin(phi2) - sin(phi1) * cos(phi2) * cos(dLon);
  double brng = atan2(y, x) * kRadToDeg;
  return fmodf(static_cast<float>(brng) + 360.0f, 360.0f);
}

void copyTrimmed(char* dst, size_t dstSize, const char* src) {
  if (!src) {
    dst[0] = '\0';
    return;
  }
  while (*src == ' ') {
    ++src;
  }
  strlcpy(dst, src, dstSize);
  size_t len = strlen(dst);
  while (len > 0 && dst[len - 1] == ' ') {
    dst[--len] = '\0';
  }
}

struct HostBackoff {
  unsigned long cooldownUntilMs = 0;
  uint8_t streak = 0;
};

HostBackoff airplanesBackoff;
HostBackoff adsbLolBackoff;

bool isCooldownActive(const HostBackoff& b, unsigned long now) {
  return b.cooldownUntilMs != 0 && static_cast<long>(now - b.cooldownUntilMs) < 0;
}

void recordSuccess(HostBackoff& b) {
  b.cooldownUntilMs = 0;
  b.streak = 0;
}

void recordFailure(HostBackoff& b, int httpCode, const char* host, unsigned long now) {
  unsigned long backoffMs = 0;
  if (httpCode == 429) {
    uint8_t shift = (b.streak < 4) ? b.streak : 4;
    backoffMs = 60000UL << shift; // 60s, 120s, 240s, 480s, 960s
    if (backoffMs > 900000UL) backoffMs = 900000UL;
    if (b.streak < 10) ++b.streak;
  } else if (httpCode == 403) {
    backoffMs = (b.streak == 0) ? 300000UL : 900000UL; // 5m then 15m
    if (b.streak < 10) ++b.streak;
  }
  if (backoffMs > 0) {
    b.cooldownUntilMs = now + backoffMs;
    Serial.printf("[Flights] %s returned HTTP %d; backing off for %lus\n",
                  host,
                  httpCode,
                  backoffMs / 1000UL);
  }
}

char cachedCallsign[16] = {0};
char cachedOrigin[8] = {0};
char cachedDest[8] = {0};

bool fetchRouteFromAdsbDb(const char* callsign,
                          char* originOut,
                          size_t originSize,
                          char* destOut,
                          size_t destSize) {
  char url[96];
  snprintf(url, sizeof(url), "https://api.adsbdb.com/v0/callsign/%s", callsign);

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(5000);
  http.setUserAgent("ESP32-FlightClock/1.0");
  if (!http.begin(client, url)) {
    return false;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  JsonDocument filter;
  filter["response"]["flightroute"]["origin"]["iata_code"] = true;
  filter["response"]["flightroute"]["origin"]["icao_code"] = true;
  filter["response"]["flightroute"]["destination"]["iata_code"] = true;
  filter["response"]["flightroute"]["destination"]["icao_code"] = true;

  JsonDocument doc;
  if (deserializeJson(doc, payload, DeserializationOption::Filter(filter))) {
    return false;
  }

  JsonObject route = doc["response"]["flightroute"];
  if (route.isNull()) {
    return false;
  }

  const char* orig = route["origin"]["iata_code"] | "";
  if (orig[0] == '\0') orig = route["origin"]["icao_code"] | "";
  const char* dest = route["destination"]["iata_code"] | "";
  if (dest[0] == '\0') dest = route["destination"]["icao_code"] | "";
  if (orig[0] == '\0' || dest[0] == '\0') {
    return false;
  }

  strlcpy(originOut, orig, originSize);
  strlcpy(destOut, dest, destSize);
  return true;
}

bool fetchRouteFromAdsbLol(const char* callsign,
                           double lat,
                           double lon,
                           char* originOut,
                           size_t originSize,
                           char* destOut,
                           size_t destSize) {
  unsigned long now = millis();
  if (isCooldownActive(adsbLolBackoff, now)) {
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(5000);
  http.setUserAgent("ESP32-FlightClock/1.0");
  if (!http.begin(client, "https://api.adsb.lol/api/0/routeset")) {
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");
#ifdef ADSB_LOL_API_KEY
  if (ADSB_LOL_API_KEY[0] != '\0') {
    http.addHeader("x-api-key", ADSB_LOL_API_KEY);
  }
#endif

  char body[128];
  snprintf(body,
           sizeof(body),
           "{\"planes\":[{\"callsign\":\"%s\",\"lat\":%.4f,\"lng\":%.4f}]}",
           callsign,
           lat,
           lon);

  int httpCode = http.POST(reinterpret_cast<uint8_t*>(body), strlen(body));
  if (httpCode != HTTP_CODE_OK) {
    http.end();
    recordFailure(adsbLolBackoff, httpCode, "api.adsb.lol", now);
    return false;
  }
  recordSuccess(adsbLolBackoff);

  String payload = http.getString();
  http.end();

  JsonDocument filter;
  filter[0]["airport_codes"] = true;
  filter[0]["_airports"][0]["iata"] = true;
  filter[0]["_airports"][0]["icao"] = true;

  JsonDocument doc;
  if (deserializeJson(doc, payload, DeserializationOption::Filter(filter))) {
    return false;
  }

  JsonObject entry = doc[0];
  if (entry.isNull()) {
    return false;
  }

  const char* orig = "";
  const char* dest = "";
  JsonArray airports = entry["_airports"].as<JsonArray>();
  if (!airports.isNull() && airports.size() >= 2) {
    JsonObject first = airports[0];
    JsonObject last = airports[airports.size() - 1];
    orig = first["iata"] | "";
    if (orig[0] == '\0') orig = first["icao"] | "";
    dest = last["iata"] | "";
    if (dest[0] == '\0') dest = last["icao"] | "";
  }

  if (orig[0] != '\0' && dest[0] != '\0') {
    strlcpy(originOut, orig, originSize);
    strlcpy(destOut, dest, destSize);
    return true;
  }

  const char* codes = entry["airport_codes"] | "";
  const char* dash = strchr(codes, '-');
  if (dash && dash != codes && dash[1] != '\0') {
    size_t origLen = static_cast<size_t>(dash - codes);
    if (origLen >= originSize) origLen = originSize - 1;
    memcpy(originOut, codes, origLen);
    originOut[origLen] = '\0';
    strlcpy(destOut, dash + 1, destSize);
    return (originOut[0] != '\0' && destOut[0] != '\0');
  }

  return false;
}

void fetchRoute(const char* callsign,
                double lat,
                double lon,
                char* originOut,
                size_t originSize,
                char* destOut,
                size_t destSize) {
  originOut[0] = '\0';
  destOut[0] = '\0';

  if (!callsign || callsign[0] == '\0' || strcmp(callsign, "UNKNOWN") == 0) {
    return;
  }

  if (strcmp(callsign, cachedCallsign) == 0) {
    strlcpy(originOut, cachedOrigin, originSize);
    strlcpy(destOut, cachedDest, destSize);
    return;
  }

  strlcpy(cachedCallsign, callsign, sizeof(cachedCallsign));
  cachedOrigin[0] = '\0';
  cachedDest[0] = '\0';

  if (!fetchRouteFromAdsbDb(callsign, cachedOrigin, sizeof(cachedOrigin), cachedDest, sizeof(cachedDest))) {
    fetchRouteFromAdsbLol(callsign, lat, lon, cachedOrigin, sizeof(cachedOrigin), cachedDest, sizeof(cachedDest));
  }

  strlcpy(originOut, cachedOrigin, originSize);
  strlcpy(destOut, cachedDest, destSize);
}

bool fetchFromHost(const char* host,
                   double lat,
                   double lon,
                   float radiusNm,
                   String& payloadOut,
                   int& httpCodeOut) {
  char url[192];
  snprintf(url,
           sizeof(url),
           "https://%s/v2/point/%.4f/%.4f/%.1f",
           host,
           lat,
           lon,
           radiusNm);

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(10000);
  const bool isAdsbLol = (strcmp(host, "api.adsb.lol") == 0);
  http.setUserAgent(isAdsbLol ? "ESP32-FlightClock/1.0" : "curl/8.5.0");
  if (!http.begin(client, url)) {
    Serial.printf("[Flights] http.begin failed (%s)\n", host);
    httpCodeOut = -1;
    return false;
  }
  http.addHeader("Accept", "application/json");
#ifdef ADSB_LOL_API_KEY
  if (isAdsbLol && ADSB_LOL_API_KEY[0] != '\0') {
    http.addHeader("x-api-key", ADSB_LOL_API_KEY);
  }
#endif

  httpCodeOut = http.GET();
  if (httpCodeOut != HTTP_CODE_OK) {
    Serial.printf("[Flights] HTTP %d (%s) from %s (freeHeap=%u)\n",
                  httpCodeOut,
                  http.errorToString(httpCodeOut).c_str(),
                  host,
                  static_cast<unsigned>(ESP.getFreeHeap()));
    http.end();
    return false;
  }

  payloadOut = http.getString();
  http.end();
  return true;
}

} // namespace

namespace Flights {

bool fetchClosest(const AppConfig& cfg, FlightInfo& out) {
  if (!cfg.locationConfigured) {
    return false;
  }

  float radiusNm = cfg.searchRadiusKm * 0.539957f;
  if (radiusNm < 1.0f) radiusNm = 1.0f;
  if (radiusNm > 150.0f) radiusNm = 150.0f;

  bool primaryIsAdsbLol = (cfg.flightSource == "adsb.lol");
  const char* primaryHost = primaryIsAdsbLol ? "api.adsb.lol" : "api.airplanes.live";
  const char* fallbackHost = primaryIsAdsbLol ? "api.airplanes.live" : "api.adsb.lol";
  HostBackoff& primaryBackoff = primaryIsAdsbLol ? adsbLolBackoff : airplanesBackoff;
  HostBackoff& fallbackBackoff = primaryIsAdsbLol ? airplanesBackoff : adsbLolBackoff;

  unsigned long now = millis();
  String payload;
  bool gotPayload = false;

  if (!isCooldownActive(primaryBackoff, now)) {
    int code = 0;
    if (fetchFromHost(primaryHost, cfg.latitude, cfg.longitude, radiusNm, payload, code)) {
      recordSuccess(primaryBackoff);
      gotPayload = true;
    } else {
      recordFailure(primaryBackoff, code, primaryHost, now);
    }
  }

  if (!gotPayload && !isCooldownActive(fallbackBackoff, now)) {
    int code = 0;
    if (fetchFromHost(fallbackHost, cfg.latitude, cfg.longitude, radiusNm, payload, code)) {
      recordSuccess(fallbackBackoff);
      gotPayload = true;
    } else {
      recordFailure(fallbackBackoff, code, fallbackHost, now);
    }
  }

  if (!gotPayload) {
    return false;
  }

  JsonDocument filter;
  JsonObject acFilter = filter["ac"].add<JsonObject>();
  acFilter["flight"] = true;
  acFilter["r"] = true;
  acFilter["t"] = true;
  acFilter["alt_baro"] = true;
  acFilter["gs"] = true;
  acFilter["track"] = true;
  acFilter["baro_rate"] = true;
  acFilter["lat"] = true;
  acFilter["lon"] = true;
  acFilter["squawk"] = true;

  JsonDocument doc;
  DeserializationError err =
      deserializeJson(doc, payload, DeserializationOption::Filter(filter));

  if (err) {
    Serial.printf("[Flights] JSON error: %s\n", err.c_str());
    return false;
  }

  JsonArray ac = doc["ac"].as<JsonArray>();
  if (ac.isNull() || ac.size() == 0) {
    out.active = false;
    out.totalInRange = 0;
    return true;
  }

  float bestDistKm = 1e9f;
  int countInRange = 0;
  FlightInfo best;

  double bestLat = 0.0;
  double bestLon = 0.0;

  for (JsonObject plane : ac) {
    if (!plane["lat"].is<double>() || !plane["lon"].is<double>()) {
      continue;
    }
    // Skip aircraft explicitly reported on the ground
    if (plane["alt_baro"].is<const char*>()) {
      continue;
    }
    int32_t altFt = plane["alt_baro"] | 0;
    if (altFt <= 0 || altFt > cfg.maxAltitudeFt) {
      continue;
    }

    double pLat = plane["lat"].as<double>();
    double pLon = plane["lon"].as<double>();
    float distKm = haversineKm(cfg.latitude, cfg.longitude, pLat, pLon);
    if (distKm > cfg.searchRadiusKm) {
      continue;
    }

    ++countInRange;
    if (distKm < bestDistKm) {
      bestDistKm = distKm;
      best.active = true;
      copyTrimmed(best.callsign, sizeof(best.callsign), plane["flight"] | "");
      if (best.callsign[0] == '\0') {
        copyTrimmed(best.callsign, sizeof(best.callsign), plane["r"] | "UNKNOWN");
      }
      copyTrimmed(best.registration, sizeof(best.registration), plane["r"] | "");
      copyTrimmed(best.aircraftType, sizeof(best.aircraftType), plane["t"] | "");
      best.altitudeFt = altFt;
      best.verticalRateFpm = plane["baro_rate"] | 0;
      best.speedKnots = plane["gs"] | 0.0f;
      best.trackDeg = plane["track"] | 0.0f;
      best.distanceKm = distKm;
      best.bearingDeg = initialBearingDeg(cfg.latitude, cfg.longitude, pLat, pLon);
      best.lastSeenMs = millis();
      bestLat = pLat;
      bestLon = pLon;
    }
  }

  if (countInRange > 0) {
    fetchRoute(best.callsign,
               bestLat,
               bestLon,
               best.origin,
               sizeof(best.origin),
               best.destination,
               sizeof(best.destination));
    best.totalInRange = countInRange;
    out = best;
  } else {
    out.active = false;
    out.totalInRange = 0;
  }
  return true;
}

} // namespace Flights
