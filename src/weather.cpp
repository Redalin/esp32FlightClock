#include "weather.h"
#include "logger.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

namespace Weather {

bool fetch(const AppConfig& cfg, WeatherData& out) {
  if (!cfg.locationConfigured) {
    return false;
  }

  static int cachedDailyMday = -1;
  static double cachedLat = 0.0;
  static double cachedLon = 0.0;

  struct tm ti;
  int todayMday = (getLocalTime(&ti, 5) && ti.tm_year > (2020 - 1900)) ? ti.tm_mday : -1;
  bool needDaily = !out.valid ||
                   (todayMday < 0) ||
                   (todayMday != cachedDailyMday) ||
                   (cfg.latitude != cachedLat) ||
                   (cfg.longitude != cachedLon);

  WiFiClientSecure client;
  client.setInsecure();

  char url[320];
  if (needDaily) {
    snprintf(
        url,
        sizeof(url),
        "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
        "&current=temperature_2m,relative_humidity_2m,is_day,weather_code"
        "&daily=temperature_2m_max,temperature_2m_min&forecast_days=1&timezone=auto",
        cfg.latitude,
        cfg.longitude);
  } else {
    snprintf(
        url,
        sizeof(url),
        "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
        "&current=temperature_2m,relative_humidity_2m,is_day,weather_code",
        cfg.latitude,
        cfg.longitude);
  }

  HTTPClient http;
  http.useHTTP10(true); // Avoid chunked transfer encoding on getStream()
  http.setConnectTimeout(4000);
  http.setTimeout(6000);
  if (!http.begin(client, url)) {
    Log::println("[Weather] http.begin failed");
    return false;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Log::printf("[Weather] HTTP %d (%s), freeHeap=%u\n",
                httpCode,
                http.errorToString(httpCode).c_str(),
                static_cast<unsigned>(ESP.getFreeHeap()));
    http.end();
    return false;
  }

  JsonDocument filter;
  filter["current"]["temperature_2m"] = true;
  filter["current"]["relative_humidity_2m"] = true;
  filter["current"]["is_day"] = true;
  filter["current"]["weather_code"] = true;
  if (needDaily) {
    filter["daily"]["temperature_2m_max"][0] = true;
    filter["daily"]["temperature_2m_min"][0] = true;
  }

  JsonDocument doc;
  DeserializationError err =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();

  if (err) {
    Log::printf("[Weather] JSON error: %s\n", err.c_str());
    return false;
  }

  JsonObject current = doc["current"];
  if (current.isNull()) {
    return false;
  }

  out.tempC = current["temperature_2m"] | 0.0f;
  out.humidity = current["relative_humidity_2m"] | 0;
  out.isDay = (current["is_day"] | 1) == 1;
  out.weatherCode = current["weather_code"] | 0;

  if (needDaily) {
    out.tempMaxC = doc["daily"]["temperature_2m_max"][0] | out.tempC;
    out.tempMinC = doc["daily"]["temperature_2m_min"][0] | out.tempC;
    cachedDailyMday = todayMday;
    cachedLat = cfg.latitude;
    cachedLon = cfg.longitude;
  }
  out.valid = true;
  out.updatedAtMs = millis();
  return true;
}

const char* describeCode(int wmoCode) {
  switch (wmoCode) {
    case 0: return "Clear";
    case 1: return "Mostly Clear";
    case 2: return "Partly Cloudy";
    case 3: return "Overcast";
    case 45:
    case 48: return "Fog";
    case 51:
    case 53:
    case 55: return "Drizzle";
    case 56:
    case 57: return "Freezing Drizzle";
    case 61:
    case 63:
    case 65: return "Rain";
    case 66:
    case 67: return "Freezing Rain";
    case 71:
    case 73:
    case 75:
    case 77: return "Snow";
    case 80:
    case 81:
    case 82: return "Showers";
    case 85:
    case 86: return "Snow Showers";
    case 95:
    case 96:
    case 99: return "Thunderstorm";
    default: return "Unknown";
  }
}

} // namespace Weather
