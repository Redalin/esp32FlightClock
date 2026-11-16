#include "weather.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

namespace Weather {

bool fetch(const AppConfig& cfg, WeatherData& out) {
  if (!cfg.locationConfigured) {
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  char url[256];
  snprintf(
      url,
      sizeof(url),
      "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
      "&current=temperature_2m,apparent_temperature,relative_humidity_2m,"
      "is_day,weather_code,wind_speed_10m"
      "&daily=temperature_2m_max,temperature_2m_min&forecast_days=1&timezone=auto",
      cfg.latitude,
      cfg.longitude);

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
  filter["current"]["temperature_2m"] = true;
  filter["current"]["apparent_temperature"] = true;
  filter["current"]["relative_humidity_2m"] = true;
  filter["current"]["is_day"] = true;
  filter["current"]["weather_code"] = true;
  filter["current"]["wind_speed_10m"] = true;
  filter["daily"]["temperature_2m_max"][0] = true;
  filter["daily"]["temperature_2m_min"][0] = true;

  JsonDocument doc;
  DeserializationError err =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();

  if (err) {
    return false;
  }

  JsonObject current = doc["current"];
  if (current.isNull()) {
    return false;
  }

  out.tempC = current["temperature_2m"] | 0.0f;
  out.feelsLikeC = current["apparent_temperature"] | out.tempC;
  out.humidity = current["relative_humidity_2m"] | 0;
  out.isDay = (current["is_day"] | 1) == 1;
  out.weatherCode = current["weather_code"] | 0;
  out.windKmh = current["wind_speed_10m"] | 0.0f;

  out.tempMaxC = doc["daily"]["temperature_2m_max"][0] | out.tempC;
  out.tempMinC = doc["daily"]["temperature_2m_min"][0] | out.tempC;
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
