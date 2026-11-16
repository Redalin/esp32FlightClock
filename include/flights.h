#pragma once

#include "config.h"

struct FlightInfo {
  bool active = false;
  char callsign[16] = {0};
  char registration[12] = {0};
  char aircraftType[8] = {0};
  char squawk[8] = {0};
  int32_t altitudeFt = 0;
  int32_t verticalRateFpm = 0;
  float speedKnots = 0.0f;
  float trackDeg = 0.0f;
  float distanceKm = 0.0f;
  float bearingDeg = 0.0f;
  int totalInRange = 0;
  unsigned long lastSeenMs = 0;
};

namespace Flights {
  bool fetchClosest(const AppConfig& cfg, FlightInfo& out);
}
