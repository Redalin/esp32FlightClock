#include "flights.h"
#include "logger.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include <math.h>
#include <stddef.h>

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

HostBackoff adsbFiBackoff;
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
  } else if (httpCode < 0) {
    uint8_t shift = (b.streak < 2) ? b.streak : 2;
    backoffMs = 15000UL << shift; // 15s, 30s, 60s
    if (b.streak < 10) ++b.streak;
  }
  if (backoffMs > 0) {
    b.cooldownUntilMs = now + backoffMs;
    Log::printf("[Flights] %s returned HTTP %d; backing off for %lus\n",
                host,
                httpCode,
                backoffMs / 1000UL);
  }
}

// Streaming reader that handles both Content-Length and HTTP/1.1 chunked responses
// so ArduinoJson can parse directly off the socket while keeping Keep-Alive aligned.
class HttpBodyStream : public Stream {
public:
  HttpBodyStream(WiFiClient& client, int contentLength, uint32_t timeoutMs = 5000)
      : _client(client),
        _chunked(contentLength < 0),
        _remaining(contentLength >= 0 ? static_cast<size_t>(contentLength) : 0),
        _timeoutMs(timeoutMs),
        _eof(contentLength == 0) {}

  int available() override {
    if (_eof) return 0;
    if (!_chunked) {
      int avail = _client.available();
      return (static_cast<size_t>(avail) > _remaining) ? static_cast<int>(_remaining) : avail;
    }
    if (_chunkRemaining > 0) {
      int avail = _client.available();
      return (static_cast<size_t>(avail) > _chunkRemaining) ? static_cast<int>(_chunkRemaining) : avail;
    }
    return _client.available() > 0 ? 1 : 0;
  }

  int read() override {
    uint8_t b = 0;
    return (readBytes(reinterpret_cast<char*>(&b), 1) == 1) ? static_cast<int>(b) : -1;
  }

  int peek() override {
    if (_eof) return -1;
    return _client.peek();
  }

  size_t write(uint8_t) override { return 0; }

  size_t readBytes(char* buffer, size_t length) {
    if (_eof || length == 0) return 0;
    size_t total = 0;
    while (total < length && !_eof) {
      if (_chunked) {
        if (_chunkRemaining == 0) {
          if (!readNextChunkHeader()) {
            break;
          }
          if (_eof) {
            break;
          }
        }
        size_t toRead = length - total;
        if (toRead > _chunkRemaining) toRead = _chunkRemaining;
        size_t got = readRaw(buffer + total, toRead);
        total += got;
        _chunkRemaining -= got;
        if (got == 0) {
          _error = true;
          _eof = true;
          break;
        }
        if (_chunkRemaining == 0) {
          if (!consumeLine()) {
            _error = true;
            _eof = true;
            break;
          }
        }
      } else {
        if (_remaining == 0) {
          _eof = true;
          break;
        }
        size_t toRead = length - total;
        if (toRead > _remaining) toRead = _remaining;
        size_t got = readRaw(buffer + total, toRead);
        total += got;
        _remaining -= got;
        if (got == 0) {
          _error = true;
          _eof = true;
          break;
        }
        if (_remaining == 0) {
          _eof = true;
        }
      }
    }
    return total;
  }

  bool drainToEof() {
    char discard[64];
    while (!_eof) {
      if (readBytes(discard, sizeof(discard)) == 0) {
        break;
      }
    }
    return _eof && !_error;
  }

  bool hadError() const { return _error; }

private:
  int readByteBlocking() {
    unsigned long start = millis();
    while (true) {
      int c = _client.read();
      if (c >= 0) return c;
      if (!_client.connected() && _client.available() == 0) {
        return -1;
      }
      if ((millis() - start) >= _timeoutMs) {
        return -1;
      }
      delay(1);
    }
  }

  size_t readRaw(char* buf, size_t len) {
    unsigned long start = millis();
    while (_client.available() == 0) {
      if (!_client.connected()) {
        return 0;
      }
      if ((millis() - start) >= _timeoutMs) {
        return 0;
      }
      delay(1);
    }
    int avail = _client.available();
    size_t n = (static_cast<size_t>(avail) < len) ? static_cast<size_t>(avail) : len;
    int got = _client.read(reinterpret_cast<uint8_t*>(buf), n);
    return (got > 0) ? static_cast<size_t>(got) : 0;
  }

  bool consumeLine() {
    while (true) {
      int c = readByteBlocking();
      if (c < 0) return false;
      if (c == '\n') return true;
    }
  }

  bool readNextChunkHeader() {
    size_t chunkSize = 0;
    bool gotHex = false;
    bool inExt = false;
    while (true) {
      int c = readByteBlocking();
      if (c < 0) {
        _error = true;
        _eof = true;
        return false;
      }
      if (c == '\n') {
        if (gotHex) break;
        continue;
      }
      if (c == '\r') continue;
      if (c == ';') {
        inExt = true;
        continue;
      }
      if (inExt) continue;
      int digit = -1;
      if (c >= '0' && c <= '9') digit = c - '0';
      else if (c >= 'a' && c <= 'f') digit = 10 + (c - 'a');
      else if (c >= 'A' && c <= 'F') digit = 10 + (c - 'A');
      if (digit < 0) {
        _error = true;
        _eof = true;
        return false;
      }
      gotHex = true;
      chunkSize = (chunkSize << 4) | static_cast<size_t>(digit);
    }
    if (chunkSize == 0) {
      // Consume trailing CRLF (and any optional trailer headers) until empty line
      size_t lineLen = 0;
      while (true) {
        int c = readByteBlocking();
        if (c < 0) {
          _error = true;
          break;
        }
        if (c == '\r') continue;
        if (c == '\n') {
          if (lineLen == 0) break;
          lineLen = 0;
        } else {
          ++lineLen;
        }
      }
      _eof = true;
      return true;
    }
    _chunkRemaining = chunkSize;
    return true;
  }

  WiFiClient& _client;
  bool _chunked;
  size_t _remaining;
  size_t _chunkRemaining = 0;
  uint32_t _timeoutMs;
  bool _eof = false;
  bool _error = false;
};

enum class RouteCacheStatus : uint8_t {
  EMPTY = 0,
  FOUND,
  NOT_FOUND,
  RETRY_LATER
};

struct RouteCacheEntry {
  char callsign[16] = {0};
  char origin[8] = {0};
  char dest[8] = {0};
  unsigned long expiresAtMs = 0;
  unsigned long lastUsedMs = 0;
  RouteCacheStatus status = RouteCacheStatus::EMPTY;
};

constexpr size_t kRouteCacheSize = 64;
constexpr unsigned long kRouteFoundTtlMs = 24UL * 60UL * 60UL * 1000UL;   // 24 hours
constexpr unsigned long kRouteNotFoundTtlMs = 2UL * 60UL * 60UL * 1000UL; // 2 hours
constexpr unsigned long kRouteRetryTtlMs = 5UL * 60UL * 1000UL;           // 5 minutes on 429/net error
constexpr char kRouteCacheNamespace[] = "routecache";
constexpr char kRouteCacheKey[] = "v1";
constexpr uint32_t kRouteCacheMagic = 0x52544331UL; // "RTC1"

struct PersistedRouteRecord {
  char callsign[12];
  char origin[6];
  char dest[6];
};

struct PersistedRouteBlob {
  uint32_t magic;
  uint16_t count;
  uint16_t reserved;
  PersistedRouteRecord entries[kRouteCacheSize];
};

RouteCacheEntry routeCache[kRouteCacheSize];
PersistedRouteBlob persistedRouteBlob;
bool routeCacheLoaded = false;

bool isRouteEntryFresh(const RouteCacheEntry& e, unsigned long now) {
  return e.status != RouteCacheStatus::EMPTY &&
         static_cast<long>(now - e.expiresAtMs) < 0;
}

void ensureRouteCacheLoaded(unsigned long now) {
  if (routeCacheLoaded) {
    return;
  }
  routeCacheLoaded = true;

  Preferences prefs;
  if (!prefs.begin(kRouteCacheNamespace, true)) {
    return;
  }
  size_t len = prefs.getBytesLength(kRouteCacheKey);
  constexpr size_t kHeaderSize = offsetof(PersistedRouteBlob, entries);
  if (len < kHeaderSize || len > sizeof(PersistedRouteBlob)) {
    prefs.end();
    return;
  }
  size_t readLen = prefs.getBytes(kRouteCacheKey, &persistedRouteBlob, len);
  prefs.end();

  if (readLen != len || persistedRouteBlob.magic != kRouteCacheMagic ||
      persistedRouteBlob.count > kRouteCacheSize) {
    return;
  }
  size_t expectedLen = kHeaderSize + persistedRouteBlob.count * sizeof(PersistedRouteRecord);
  if (readLen != expectedLen) {
    return;
  }

  size_t loaded = 0;
  for (uint16_t i = 0; i < persistedRouteBlob.count; ++i) {
    const PersistedRouteRecord& rec = persistedRouteBlob.entries[i];
    if (rec.callsign[0] == '\0' || rec.origin[0] == '\0' || rec.dest[0] == '\0') {
      continue;
    }
    RouteCacheEntry& slot = routeCache[loaded];
    strlcpy(slot.callsign, rec.callsign, sizeof(slot.callsign));
    strlcpy(slot.origin, rec.origin, sizeof(slot.origin));
    strlcpy(slot.dest, rec.dest, sizeof(slot.dest));
    slot.status = RouteCacheStatus::FOUND;
    slot.expiresAtMs = now + kRouteFoundTtlMs;
    slot.lastUsedMs = now - (persistedRouteBlob.count - i);
    ++loaded;
  }
  if (loaded > 0) {
    Log::printf("[Routes] Loaded %u cached routes from NVS\n", static_cast<unsigned>(loaded));
  }
}

void saveRouteCacheToNvs() {
  uint16_t count = 0;
  for (size_t i = 0; i < kRouteCacheSize; ++i) {
    const RouteCacheEntry& e = routeCache[i];
    if (e.status != RouteCacheStatus::FOUND || e.callsign[0] == '\0' ||
        e.origin[0] == '\0' || e.dest[0] == '\0') {
      continue;
    }
    PersistedRouteRecord& rec = persistedRouteBlob.entries[count++];
    strlcpy(rec.callsign, e.callsign, sizeof(rec.callsign));
    strlcpy(rec.origin, e.origin, sizeof(rec.origin));
    strlcpy(rec.dest, e.dest, sizeof(rec.dest));
  }

  persistedRouteBlob.magic = kRouteCacheMagic;
  persistedRouteBlob.count = count;
  persistedRouteBlob.reserved = 0;

  Preferences prefs;
  if (!prefs.begin(kRouteCacheNamespace, false)) {
    return;
  }
  size_t blobSize = offsetof(PersistedRouteBlob, entries) + count * sizeof(PersistedRouteRecord);
  prefs.putBytes(kRouteCacheKey, &persistedRouteBlob, blobSize);
  prefs.end();
}

size_t selectRouteCacheSlot(int existingIdx, unsigned long now) {
  if (existingIdx >= 0) {
    return static_cast<size_t>(existingIdx);
  }
  for (size_t i = 0; i < kRouteCacheSize; ++i) {
    if (routeCache[i].status == RouteCacheStatus::EMPTY) {
      return i;
    }
  }

  // Prefer evicting expired or non-FOUND entries before evicting active FOUND routes;
  // break ties by least-recently-used.
  auto evictPriority = [&](const RouteCacheEntry& e) -> uint8_t {
    if (!isRouteEntryFresh(e, now)) return 0;
    if (e.status == RouteCacheStatus::RETRY_LATER) return 1;
    if (e.status == RouteCacheStatus::NOT_FOUND) return 2;
    return 3; // Fresh FOUND route
  };

  size_t victim = 0;
  uint8_t victimPrio = evictPriority(routeCache[0]);
  for (size_t i = 1; i < kRouteCacheSize; ++i) {
    uint8_t prio = evictPriority(routeCache[i]);
    if (prio < victimPrio ||
        (prio == victimPrio &&
         static_cast<long>(routeCache[i].lastUsedMs - routeCache[victim].lastUsedMs) < 0)) {
      victim = i;
      victimPrio = prio;
    }
  }
  return victim;
}

enum class RouteLookupResult : uint8_t {
  FOUND = 0,
  NOT_FOUND,
  NET_ERROR
};

// Persistent TLS client + HTTPClient for the active ADS-B feed host (Keep-Alive)
WiFiClientSecure flightTlsClient;
HTTPClient flightHttp;
bool flightClientInit = false;
char lastFlightHost[48] = {0};

void closeFlightConnection() {
  flightHttp.end();
  flightTlsClient.stop();
  lastFlightHost[0] = '\0';
}

RouteLookupResult fetchRouteFromAdsbDb(const char* callsign,
                                       char* originOut,
                                       size_t originSize,
                                       char* destOut,
                                       size_t destSize) {
  char url[96];
  snprintf(url, sizeof(url), "https://api.adsbdb.com/v0/callsign/%s", callsign);

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.useHTTP10(true);
  http.setConnectTimeout(3500);
  http.setTimeout(4000);
  http.setUserAgent("ESP32-FlightClock/1.0");
  if (!http.begin(client, url)) {
    return RouteLookupResult::NET_ERROR;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    http.end();
    return (httpCode < 0 || httpCode >= 500 || httpCode == 429)
               ? RouteLookupResult::NET_ERROR
               : RouteLookupResult::NOT_FOUND;
  }

  static JsonDocument filter;
  static bool filterInit = false;
  if (!filterInit) {
    filter["response"]["flightroute"]["origin"]["iata_code"] = true;
    filter["response"]["flightroute"]["origin"]["icao_code"] = true;
    filter["response"]["flightroute"]["destination"]["iata_code"] = true;
    filter["response"]["flightroute"]["destination"]["icao_code"] = true;
    filterInit = true;
  }

  JsonDocument doc;
  DeserializationError err =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    return RouteLookupResult::NET_ERROR;
  }

  JsonObject route = doc["response"]["flightroute"];
  if (route.isNull()) {
    return RouteLookupResult::NOT_FOUND;
  }

  const char* orig = route["origin"]["iata_code"] | "";
  if (orig[0] == '\0') orig = route["origin"]["icao_code"] | "";
  const char* dest = route["destination"]["iata_code"] | "";
  if (dest[0] == '\0') dest = route["destination"]["icao_code"] | "";
  if (orig[0] == '\0' || dest[0] == '\0') {
    return RouteLookupResult::NOT_FOUND;
  }

  strlcpy(originOut, orig, originSize);
  strlcpy(destOut, dest, destSize);
  return RouteLookupResult::FOUND;
}

RouteLookupResult fetchRouteFromAdsbLol(const char* callsign,
                                        double lat,
                                        double lon,
                                        char* originOut,
                                        size_t originSize,
                                        char* destOut,
                                        size_t destSize) {
  unsigned long now = millis();
  if (isCooldownActive(adsbLolBackoff, now)) {
    return RouteLookupResult::NET_ERROR;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.useHTTP10(true);
  http.setConnectTimeout(3500);
  http.setTimeout(4000);
  http.setUserAgent("ESP32-FlightClock/1.0");
  if (!http.begin(client, "https://api.adsb.lol/api/0/routeset")) {
    return RouteLookupResult::NET_ERROR;
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
    return (httpCode < 0 || httpCode >= 500 || httpCode == 429 || httpCode == 403)
               ? RouteLookupResult::NET_ERROR
               : RouteLookupResult::NOT_FOUND;
  }
  recordSuccess(adsbLolBackoff);

  static JsonDocument filter;
  static bool filterInit = false;
  if (!filterInit) {
    filter[0]["airport_codes"] = true;
    filter[0]["_airports"][0]["iata"] = true;
    filter[0]["_airports"][0]["icao"] = true;
    filterInit = true;
  }

  JsonDocument doc;
  DeserializationError err =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    return RouteLookupResult::NET_ERROR;
  }

  JsonObject entry = doc[0];
  if (entry.isNull()) {
    return RouteLookupResult::NOT_FOUND;
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
    return RouteLookupResult::FOUND;
  }

  const char* codes = entry["airport_codes"] | "";
  const char* dash = strchr(codes, '-');
  if (dash && dash != codes && dash[1] != '\0') {
    size_t origLen = static_cast<size_t>(dash - codes);
    if (origLen >= originSize) origLen = originSize - 1;
    memcpy(originOut, codes, origLen);
    originOut[origLen] = '\0';
    strlcpy(destOut, dash + 1, destSize);
    if (originOut[0] != '\0' && destOut[0] != '\0') {
      return RouteLookupResult::FOUND;
    }
  }

  return RouteLookupResult::NOT_FOUND;
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

  unsigned long now = millis();
  ensureRouteCacheLoaded(now);

  int existingIdx = -1;
  for (size_t i = 0; i < kRouteCacheSize; ++i) {
    if (routeCache[i].status != RouteCacheStatus::EMPTY &&
        strcmp(callsign, routeCache[i].callsign) == 0) {
      existingIdx = static_cast<int>(i);
      if (isRouteEntryFresh(routeCache[i], now)) {
        routeCache[i].lastUsedMs = now;
        if (routeCache[i].status == RouteCacheStatus::FOUND) {
          strlcpy(originOut, routeCache[i].origin, originSize);
          strlcpy(destOut, routeCache[i].dest, destSize);
        }
        return;
      }
      break;
    }
  }

  char origBuf[8] = {0};
  char destBuf[8] = {0};
  RouteLookupResult r1 = fetchRouteFromAdsbDb(callsign, origBuf, sizeof(origBuf), destBuf, sizeof(destBuf));
  RouteLookupResult r2 = RouteLookupResult::NOT_FOUND;
  if (r1 != RouteLookupResult::FOUND) {
    r2 = fetchRouteFromAdsbLol(callsign, lat, lon, origBuf, sizeof(origBuf), destBuf, sizeof(destBuf));
  }

  RouteCacheStatus newStatus = RouteCacheStatus::RETRY_LATER;
  unsigned long ttlMs = kRouteRetryTtlMs;
  if (r1 == RouteLookupResult::FOUND || r2 == RouteLookupResult::FOUND) {
    newStatus = RouteCacheStatus::FOUND;
    ttlMs = kRouteFoundTtlMs;
  } else if (r1 == RouteLookupResult::NOT_FOUND && r2 == RouteLookupResult::NOT_FOUND) {
    newStatus = RouteCacheStatus::NOT_FOUND;
    ttlMs = kRouteNotFoundTtlMs;
  }

  now = millis();
  size_t slotIdx = selectRouteCacheSlot(existingIdx, now);
  RouteCacheEntry& slot = routeCache[slotIdx];

  bool shouldPersist = false;
  if (newStatus == RouteCacheStatus::FOUND) {
    bool sameAsExisting = (slot.status == RouteCacheStatus::FOUND &&
                           strcmp(slot.callsign, callsign) == 0 &&
                           strcmp(slot.origin, origBuf) == 0 &&
                           strcmp(slot.dest, destBuf) == 0);
    shouldPersist = !sameAsExisting;
  }

  strlcpy(slot.callsign, callsign, sizeof(slot.callsign));
  strlcpy(slot.origin, origBuf, sizeof(slot.origin));
  strlcpy(slot.dest, destBuf, sizeof(slot.dest));
  slot.status = newStatus;
  slot.expiresAtMs = now + ttlMs;
  slot.lastUsedMs = now;

  if (shouldPersist) {
    saveRouteCacheToNvs();
  }

  strlcpy(originOut, origBuf, originSize);
  strlcpy(destOut, destBuf, destSize);
}

bool fetchFromHost(const char* host,
                   double lat,
                   double lon,
                   float radiusNm,
                   const JsonDocument& filter,
                   JsonDocument& docOut,
                   int& httpCodeOut) {
  const bool isAdsbFi = (strcmp(host, "opendata.adsb.fi") == 0);
  const bool isAdsbLol = (strcmp(host, "api.adsb.lol") == 0);

  char url[192];
  if (isAdsbFi) {
    snprintf(url,
             sizeof(url),
             "https://%s/api/v3/lat/%.4f/lon/%.4f/dist/%.1f",
             host,
             lat,
             lon,
             radiusNm);
  } else {
    snprintf(url,
             sizeof(url),
             "https://%s/v2/point/%.4f/%.4f/%.1f",
             host,
             lat,
             lon,
             radiusNm);
  }

  if (!flightClientInit) {
    flightTlsClient.setInsecure();
    flightHttp.setReuse(true);
    flightClientInit = true;
  }

  if (strcmp(lastFlightHost, host) != 0) {
    closeFlightConnection();
    strlcpy(lastFlightHost, host, sizeof(lastFlightHost));
  }

  flightHttp.setConnectTimeout(4000);
  flightHttp.setTimeout(5000);
  flightHttp.setUserAgent((isAdsbFi || isAdsbLol) ? "ESP32-FlightClock/1.0" : "curl/8.5.0");
  if (!flightHttp.begin(flightTlsClient, url)) {
    Log::printf("[Flights] http.begin failed (%s)\n", host);
    closeFlightConnection();
    httpCodeOut = -1;
    return false;
  }
  flightHttp.addHeader("Accept", "application/json");
#ifdef ADSB_LOL_API_KEY
  if (isAdsbLol && ADSB_LOL_API_KEY[0] != '\0') {
    flightHttp.addHeader("x-api-key", ADSB_LOL_API_KEY);
  }
#endif

  httpCodeOut = flightHttp.GET();
  if (httpCodeOut != HTTP_CODE_OK) {
    Log::printf("[Flights] HTTP %d (%s) from %s (freeHeap=%u)\n",
                httpCodeOut,
                flightHttp.errorToString(httpCodeOut).c_str(),
                host,
                static_cast<unsigned>(ESP.getFreeHeap()));
    closeFlightConnection();
    return false;
  }

  HttpBodyStream bodyStream(flightHttp.getStream(), flightHttp.getSize(), 5000);
  DeserializationError err =
      deserializeJson(docOut, bodyStream, DeserializationOption::Filter(filter));
  bool cleanEof = bodyStream.drainToEof();
  flightHttp.end();

  if (err || !cleanEof || bodyStream.hadError()) {
    closeFlightConnection();
    if (err) {
      Log::printf("[Flights] JSON error from %s: %s\n", host, err.c_str());
      return false;
    }
  }
  return true;
}

} // namespace

namespace Flights {

void resetConnection() {
  closeFlightConnection();
}

bool fetchClosest(const AppConfig& cfg, FlightInfo& out) {
  if (!cfg.locationConfigured) {
    return false;
  }

  float radiusNm = cfg.searchRadiusKm * 0.539957f;
  if (radiusNm < 1.0f) radiusNm = 1.0f;
  if (radiusNm > 150.0f) radiusNm = 150.0f;

  static JsonDocument filter;
  static bool filterInit = false;
  if (!filterInit) {
    auto addPlaneFields = [](JsonObject obj) {
      obj["flight"] = true;
      obj["r"] = true;
      obj["t"] = true;
      obj["alt_baro"] = true;
      obj["gs"] = true;
      obj["track"] = true;
      obj["baro_rate"] = true;
      obj["lat"] = true;
      obj["lon"] = true;
    };
    addPlaneFields(filter["ac"].add<JsonObject>());
    addPlaneFields(filter["aircraft"].add<JsonObject>());
    filterInit = true;
  }

  const char* primaryHost = "opendata.adsb.fi";
  const char* fallbackHost = nullptr;
  HostBackoff* primaryBackoff = &adsbFiBackoff;
  HostBackoff* fallbackBackoff = nullptr;

  if (cfg.flightSource == "airplanes.live") {
    primaryHost = "api.airplanes.live";
    primaryBackoff = &airplanesBackoff;
    fallbackHost = "opendata.adsb.fi";
    fallbackBackoff = &adsbFiBackoff;
  } else if (cfg.flightSource == "adsb.lol") {
    primaryHost = "api.adsb.lol";
    primaryBackoff = &adsbLolBackoff;
    fallbackHost = "opendata.adsb.fi";
    fallbackBackoff = &adsbFiBackoff;
  }

  unsigned long now = millis();
  JsonDocument doc;
  bool gotDoc = false;
  const char* usedHost = nullptr;
  bool primaryInBackoff = isCooldownActive(*primaryBackoff, now);
  bool fallbackInBackoff = fallbackBackoff ? isCooldownActive(*fallbackBackoff, now) : true;

  if (!primaryInBackoff) {
    int code = 0;
    if (fetchFromHost(primaryHost, cfg.latitude, cfg.longitude, radiusNm, filter, doc, code)) {
      recordSuccess(*primaryBackoff);
      gotDoc = true;
      usedHost = primaryHost;
    } else {
      now = millis();
      recordFailure(*primaryBackoff, code, primaryHost, now);
    }
  }

  if (!gotDoc && fallbackHost && !fallbackInBackoff) {
    int code = 0;
    if (fetchFromHost(fallbackHost, cfg.latitude, cfg.longitude, radiusNm, filter, doc, code)) {
      recordSuccess(*fallbackBackoff);
      gotDoc = true;
      usedHost = fallbackHost;
    } else {
      now = millis();
      recordFailure(*fallbackBackoff, code, fallbackHost, now);
    }
  }

  static bool lastPollWasEmpty = false;
  static unsigned long lastBackoffLogMs = 0;

  if (!gotDoc) {
    lastPollWasEmpty = false;
    if (primaryInBackoff && fallbackInBackoff &&
        (lastBackoffLogMs == 0 || (now - lastBackoffLogMs) >= 60000UL)) {
      lastBackoffLogMs = now;
      if (fallbackHost) {
        Log::println("[Flights] Poll skipped: both feeds in backoff");
      } else {
        Log::printf("[Flights] Poll skipped: %s in backoff\n", primaryHost);
      }
    }
    return false;
  }

  JsonArray ac = doc["ac"].as<JsonArray>();
  if (ac.isNull()) {
    ac = doc["aircraft"].as<JsonArray>();
  }
  if (ac.isNull() || ac.size() == 0) {
    if (!lastPollWasEmpty) {
      Log::printf("[Flights] %s: 0 aircraft within %.1fkm\n", usedHost, cfg.searchRadiusKm);
      lastPollWasEmpty = true;
    }
    out.active = false;
    out.totalInRange = 0;
    return true;
  }
  lastPollWasEmpty = false;

  float bestDistKm = 1e9f;
  int countInRange = 0;
  FlightInfo best;

  double bestLat = 0.0;
  double bestLon = 0.0;

  unsigned int noPosCount = 0;
  unsigned int groundCount = 0;
  unsigned int altCount = 0;
  unsigned int distCount = 0;
  float skippedBestDistKm = 1e9f;
  char skippedCallsign[16] = {0};
  int32_t skippedAltFt = 0;
  const char* skippedReason = nullptr;

  auto recordSkipped = [&](JsonObject& plane, float distKm, int32_t altFt, const char* reason) {
    if (distKm < skippedBestDistKm) {
      skippedBestDistKm = distKm;
      copyTrimmed(skippedCallsign, sizeof(skippedCallsign), plane["flight"] | "");
      if (skippedCallsign[0] == '\0') {
        copyTrimmed(skippedCallsign, sizeof(skippedCallsign), plane["r"] | "UNKNOWN");
      }
      skippedAltFt = altFt;
      skippedReason = reason;
    }
  };

  for (JsonObject plane : ac) {
    if (!plane["lat"].is<double>() || !plane["lon"].is<double>()) {
      ++noPosCount;
      continue;
    }
    double pLat = plane["lat"].as<double>();
    double pLon = plane["lon"].as<double>();
    float distKm = haversineKm(cfg.latitude, cfg.longitude, pLat, pLon);

    // Skip aircraft explicitly reported on the ground
    if (plane["alt_baro"].is<const char*>()) {
      ++groundCount;
      recordSkipped(plane, distKm, 0, "ground");
      continue;
    }
    int32_t altFt = plane["alt_baro"] | 0;
    if (altFt <= 0 || altFt > cfg.maxAltitudeFt) {
      ++altCount;
      recordSkipped(plane, distKm, altFt, altFt <= 0 ? "alt<=0" : "alt>max");
      continue;
    }

    if (distKm > cfg.searchRadiusKm) {
      ++distCount;
      recordSkipped(plane, distKm, altFt, "dist>rad");
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
    if (skippedReason) {
      Log::printf("[Flights] %u from %s, 0 matched (gnd=%u alt=%u dist=%u noPos=%u; nearest=%s %.1fkm %ldft [%s])\n",
                  static_cast<unsigned>(ac.size()),
                  usedHost,
                  groundCount,
                  altCount,
                  distCount,
                  noPosCount,
                  skippedCallsign,
                  skippedBestDistKm,
                  static_cast<long>(skippedAltFt),
                  skippedReason);
    } else {
      Log::printf("[Flights] %u from %s, 0 matched (noPos=%u)\n",
                  static_cast<unsigned>(ac.size()),
                  usedHost,
                  noPosCount);
    }
    out.active = false;
    out.totalInRange = 0;
  }
  return true;
}

} // namespace Flights
