#include "storage.h"
#include <Preferences.h>

namespace Storage {

void load(AppConfig& cfg) {
  Preferences prefs;
  if (!prefs.begin(CONFIG_NAMESPACE, false)) {
    return;
  }

  cfg.wifiSsid = prefs.getString("ssid", "");
  cfg.wifiPass = prefs.getString("pass", "");
  cfg.latitude = prefs.getDouble("lat", 0.0);
  cfg.longitude = prefs.getDouble("lon", 0.0);
  cfg.locationConfigured = prefs.getBool("locSet", false);
  cfg.timezone = prefs.getString("tz", "UTC0");
  cfg.use24Hour = prefs.getBool("h24", true);
  cfg.useFahrenheit = prefs.getBool("fahr", false);
  cfg.useImperialDist = prefs.getBool("imp", false);
  cfg.brightness = prefs.getUChar("bright", 220);
  cfg.searchRadiusKm = prefs.getFloat("radKm", 15.0f);
  cfg.flightPollSec = prefs.getUShort("fPoll", 15);
  cfg.maxAltitudeFt = prefs.getInt("maxAlt", 50000);
  cfg.flightSource = prefs.getString("fSrc", "airplanes.live");

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
