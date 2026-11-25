#pragma once

#include "config.h"
#include "flights.h"
#include "weather.h"

namespace PortalServer {
  struct State {
    bool wifiConnected = false;
    bool apMode = false;
    bool timeSynced = false;
    bool configUpdated = false;
    bool wifiReconnectNeeded = false;
    bool testFlightRequested = false;
    bool rebootRequested = false;
    bool otaInProgress = false;
  };

  void begin(AppConfig& cfg,
             const WeatherData& weather,
             const FlightInfo& flight,
             State& state);
  void loop();
}
