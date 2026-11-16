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

} // namespace

namespace Flights {

bool fetchClosest(const AppConfig& cfg, FlightInfo& out) {
  if (!cfg.locationConfigured) {
    return false;
  }

  float radiusNm = cfg.searchRadiusKm * 0.539957f;
  if (radiusNm < 1.0f) radiusNm = 1.0f;
  if (radiusNm > 150.0f) radiusNm = 150.0f;

  const char* host = (cfg.flightSource == "adsb.lol")
                         ? "api.adsb.lol"
                         : "api.airplanes.live";

  char url[192];
  snprintf(url,
           sizeof(url),
           "https://%s/v2/point/%.4f/%.4f/%.1f",
           host,
           cfg.latitude,
           cfg.longitude,
           radiusNm);

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(client, url)) {
    return false;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    http.end();
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
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();

  if (err) {
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
      copyTrimmed(best.squawk, sizeof(best.squawk), plane["squawk"] | "");
      best.altitudeFt = altFt;
      best.verticalRateFpm = plane["baro_rate"] | 0;
      best.speedKnots = plane["gs"] | 0.0f;
      best.trackDeg = plane["track"] | 0.0f;
      best.distanceKm = distKm;
      best.bearingDeg = initialBearingDeg(cfg.latitude, cfg.longitude, pLat, pLon);
      best.lastSeenMs = millis();
    }
  }

  if (countInRange > 0) {
    best.totalInRange = countInRange;
    out = best;
  } else {
    out.active = false;
    out.totalInRange = 0;
  }
  return true;
}

} // namespace Flights
