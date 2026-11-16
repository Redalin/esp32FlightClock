#include "storage.h"
#include <Preferences.h>

namespace Storage {

void load(AppConfig& cfg) {
#ifdef DEFAULT_WIFI_SSID
  const char* defaultSsid = DEFAULT_WIFI_SSID;
#else
  const char* defaultSsid = "";
#endif
#ifdef DEFAULT_WIFI_PASS
  const char* defaultPass = DEFAULT_WIFI_PASS;
#else
  const char* defaultPass = "";
#endif
#ifdef DEFAULT_TIMEZONE
  const char* defaultTz = DEFAULT_TIMEZONE;
#else
  const char* defaultTz = "UTC0";
#endif

  Preferences prefs;
  if (!prefs.begin(CONFIG_NAMESPACE, false)) {
    cfg.wifiSsid = defaultSsid;
    cfg.wifiPass = defaultPass;
    cfg.timezone = defaultTz;
#if defined(DEFAULT_LATITUDE) && defined(DEFAULT_LONGITUDE)
    cfg.latitude = DEFAULT_LATITUDE;
    cfg.longitude = DEFAULT_LONGITUDE;
    cfg.locationConfigured = true;
#endif
    return;
  }

  cfg.wifiSsid = prefs.getString("ssid", defaultSsid);
  cfg.wifiPass = prefs.getString("pass", defaultPass);
  cfg.latitude = prefs.getDouble("lat", 0.0);
  cfg.longitude = prefs.getDouble("lon", 0.0);
  cfg.locationConfigured = prefs.getBool("locSet", false);
#if defined(DEFAULT_LATITUDE) && defined(DEFAULT_LONGITUDE)
  if (!cfg.locationConfigured) {
    cfg.latitude = DEFAULT_LATITUDE;
    cfg.longitude = DEFAULT_LONGITUDE;
    cfg.locationConfigured = true;
  }
#endif
  cfg.timezone = prefs.getString("tz", defaultTz);
  cfg.use24Hour = prefs.getBool("h24", true);
  cfg.useFahrenheit = prefs.getBool("fahr", false);
  cfg.useImperialDist = prefs.getBool("imp", false);
  cfg.brightness = prefs.getUChar("bright", 220);
  cfg.searchRadiusKm = prefs.getFloat("radKm", 15.0f);
  cfg.flightPollSec = prefs.getUShort("fPoll", 15);
  cfg.maxAltitudeFt = prefs.getInt("maxAlt", 50000);
  cfg.flightSource = prefs.getString("fSrc", "adsb.lol");

  prefs.end();
}

void save(const AppConfig& cfg) {
  Preferences prefs;
  if (!prefs.begin(CONFIG_NAMESPACE, false)) {
    return;
  }

  prefs.putString("ssid", cfg.wifiSsid);
  prefs.putString("pass", cfg.wifiPass);
  prefs.putDouble("lat", cfg.latitude);
  prefs.putDouble("lon", cfg.longitude);
  prefs.putBool("locSet", cfg.locationConfigured);
  prefs.putString("tz", cfg.timezone);
  prefs.putBool("h24", cfg.use24Hour);
  prefs.putBool("fahr", cfg.useFahrenheit);
  prefs.putBool("imp", cfg.useImperialDist);
  prefs.putUChar("bright", cfg.brightness);
  prefs.putFloat("radKm", cfg.searchRadiusKm);
  prefs.putUShort("fPoll", cfg.flightPollSec);
  prefs.putInt("maxAlt", cfg.maxAltitudeFt);
  prefs.putString("fSrc", cfg.flightSource);

  prefs.end();
}

void reset() {
  Preferences prefs;
  if (prefs.begin(CONFIG_NAMESPACE, false)) {
    prefs.clear();
    prefs.end();
  }
}

} // namespace Storage
