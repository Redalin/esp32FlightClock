#include "display.h"
#include <LovyanGFX.hpp>
#include <math.h>
#include <time.h>

namespace {

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01 _panel_instance;
  lgfx::Bus_SPI      _bus_instance;
  lgfx::Light_PWM    _light_instance;

public:
  LGFX() {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read = 20000000;
      cfg.spi_3wire = true;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = TFT_SCLK_PIN;
      cfg.pin_mosi = TFT_MOSI_PIN;
      cfg.pin_miso = -1;
      cfg.pin_dc = TFT_DC_PIN;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = TFT_CS_PIN;
      cfg.pin_rst = TFT_RST_PIN;
      cfg.pin_busy = -1;
      cfg.panel_width = 240;
      cfg.panel_height = 240;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = false;
      cfg.invert = true;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      _panel_instance.config(cfg);
    }
    {
      auto cfg = _light_instance.config();
      cfg.pin_bl = BACKLIGHT_PIN;
      cfg.invert = false;
      cfg.freq = 5000;
      cfg.pwm_channel = 0;
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }
    setPanel(&_panel_instance);
  }
};

LGFX tft;
LGFX_Sprite spr(&tft);
bool spriteReady = false;

constexpr uint16_t COL_BG       = 0x0000; // Black
constexpr uint16_t COL_RING_DIM = 0x18E3; // Dark slate
constexpr uint16_t COL_CYAN     = 0x07FF; // Cyan
constexpr uint16_t COL_TEAL     = 0x0575; // Muted teal
constexpr uint16_t COL_AMBER    = 0xFD20; // Amber / Orange
constexpr uint16_t COL_SKY      = 0x5E5F; // Sky blue
constexpr uint16_t COL_SUN      = 0xFFE0; // Yellow
constexpr uint16_t COL_CLOUD    = 0xCE79; // Light gray
constexpr uint16_t COL_RAIN     = 0x3DDF; // Rain blue
constexpr uint16_t COL_MUTED    = 0x8410; // Mid gray
constexpr uint16_t COL_WHITE    = 0xFFFF;
constexpr uint16_t COL_GREEN    = 0x07E0;
constexpr uint16_t COL_RED      = 0xF800;

constexpr float kDegToRad = 0.0174532925f;

void drawText(const char* text, int32_t x, int32_t y, const lgfx::IFont* font) {
  spr.setFont(font);
  spr.drawString(text, x, y);
}

// 0 deg = 12 o'clock, clockwise
void polarToXY(int cx, int cy, float r, float deg, int& x, int& y) {
  float rad = deg * kDegToRad;
  x = cx + static_cast<int>(roundf(r * sinf(rad)));
  y = cy - static_cast<int>(roundf(r * cosf(rad)));
}

void drawRingArc(int cx, int cy, float r, float startDeg, float endDeg, uint16_t color, int thickness) {
  for (float a = startDeg; a <= endDeg; a += 2.0f) {
    int x, y;
    polarToXY(cx, cy, r, a, x, y);
    if (thickness <= 1) {
      spr.drawPixel(x, y, color);
    } else {
      spr.fillCircle(x, y, thickness / 2, color);
    }
  }
}

void drawSun(int cx, int cy, int r) {
  spr.fillCircle(cx, cy, r, COL_SUN);
  for (int i = 0; i < 8; ++i) {
    float a = i * 45.0f;
    int x1, y1, x2, y2;
    polarToXY(cx, cy, r + 3, a, x1, y1);
    polarToXY(cx, cy, r + 7, a, x2, y2);
    spr.drawLine(x1, y1, x2, y2, COL_SUN);
  }
}

void drawMoon(int cx, int cy, int r) {
  spr.fillCircle(cx, cy, r, COL_SUN);
  spr.fillCircle(cx + r / 2, cy - r / 4, r - 2, COL_BG);
}

void drawCloud(int cx, int cy, uint16_t color) {
  spr.fillCircle(cx - 8, cy + 2, 8, color);
  spr.fillCircle(cx + 1, cy - 3, 10, color);
  spr.fillCircle(cx + 10, cy + 2, 7, color);
  spr.fillRoundRect(cx - 16, cy + 2, 33, 9, 4, color);
}

void drawWeatherIcon(int cx, int cy, int code, bool isDay) {
  if (code == 0 || code == 1) {
    if (isDay) drawSun(cx, cy, 10);
    else drawMoon(cx, cy, 10);
    return;
  }
  if (code == 2) {
    if (isDay) drawSun(cx - 8, cy - 6, 7);
    else drawMoon(cx - 8, cy - 6, 7);
    drawCloud(cx + 3, cy + 4, COL_CLOUD);
    return;
  }
  if (code == 3) {
    drawCloud(cx, cy, COL_CLOUD);
    return;
  }
  if (code == 45 || code == 48) {
    drawCloud(cx, cy - 4, COL_MUTED);
    spr.drawFastHLine(cx - 14, cy + 9, 28, COL_CLOUD);
    spr.drawFastHLine(cx - 10, cy + 13, 20, COL_MUTED);
    return;
  }
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
    drawCloud(cx, cy - 4, COL_CLOUD);
    for (int dx = -8; dx <= 8; dx += 8) {
      spr.drawLine(cx + dx, cy + 8, cx + dx - 3, cy + 15, COL_RAIN);
    }
    return;
  }
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) {
    drawCloud(cx, cy - 4, COL_CLOUD);
    spr.fillCircle(cx - 8, cy + 11, 2, COL_WHITE);
    spr.fillCircle(cx, cy + 14, 2, COL_WHITE);
    spr.fillCircle(cx + 8, cy + 11, 2, COL_WHITE);
    return;
  }
  if (code >= 95) {
    drawCloud(cx, cy - 4, COL_MUTED);
    spr.fillTriangle(cx + 2, cy + 6, cx - 4, cy + 14, cx + 1, cy + 14, COL_SUN);
    spr.fillTriangle(cx + 1, cy + 12, cx - 2, cy + 20, cx + 5, cy + 12, COL_SUN);
    return;
  }
  drawCloud(cx, cy, COL_CLOUD);
}

const char* cardinalDir(float deg) {
  static const char* dirs[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  int idx = static_cast<int>(roundf(deg / 45.0f)) % 8;
  return dirs[idx];
}

void formatThousands(int32_t val, char* out, size_t outSize) {
  if (val < 1000 && val > -1000) {
    snprintf(out, outSize, "%ld", static_cast<long>(val));
    return;
  }
  long absVal = labs(val);
  long thousands = absVal / 1000;
  long rem = absVal % 1000;
  snprintf(out, outSize, "%s%ld,%03ld", val < 0 ? "-" : "", thousands, rem);
}

} // namespace

namespace Display {

void init(uint8_t brightness) {
  tft.init();
  tft.setRotation(0);
  tft.setBrightness(brightness);
  tft.fillScreen(COL_BG);

  spr.setColorDepth(8);
  spriteReady = (spr.createSprite(240, 240) != nullptr);
}

void setBrightness(uint8_t brightness) {
  tft.setBrightness(brightness);
}

void showBootScreen(const char* statusLine) {
  if (!spriteReady) {
    tft.fillScreen(COL_BG);
    tft.setTextColor(COL_CYAN, COL_BG);
    tft.setTextDatum(lgfx::middle_center);
    tft.setFont(&fonts::Font4);
    tft.drawString("FLIGHT CLOCK", 120, 105);
    tft.setTextColor(COL_WHITE, COL_BG);
    tft.setFont(&fonts::Font2);
    tft.drawString(statusLine, 120, 138);
    return;
  }

  spr.fillSprite(COL_BG);
  spr.drawCircle(120, 120, 114, COL_RING_DIM);
  spr.drawCircle(120, 120, 110, COL_TEAL);
  spr.setTextDatum(lgfx::middle_center);
  spr.setTextColor(COL_CYAN, COL_BG);
  drawText("FLIGHT CLOCK", 120, 104, &fonts::Font4);
  spr.setTextColor(COL_CLOUD, COL_BG);
  drawText(statusLine, 120, 138, &fonts::Font2);
  spr.pushSprite(0, 0);
}

void showPortalScreen(const char* apSsid, const char* apIp) {
  if (!spriteReady) return;

  spr.fillSprite(COL_BG);
  spr.drawCircle(120, 120, 114, COL_AMBER);
  spr.drawCircle(120, 120, 112, COL_AMBER);

  spr.setTextDatum(lgfx::middle_center);
  spr.setTextColor(COL_AMBER, COL_BG);
  drawText("SETUP MODE", 120, 56, &fonts::Font4);

  spr.setTextColor(COL_MUTED, COL_BG);
  drawText("Connect Wi-Fi to:", 120, 94, &fonts::Font2);
  spr.setTextColor(COL_WHITE, COL_BG);
  drawText(apSsid, 120, 116, &fonts::Font4);

  spr.setTextColor(COL_MUTED, COL_BG);
  drawText("Open browser to:", 120, 152, &fonts::Font2);
  spr.setTextColor(COL_CYAN, COL_BG);
  drawText(apIp, 120, 174, &fonts::Font4);

  spr.pushSprite(0, 0);
}

void renderClockWeather(const AppConfig& cfg,
                        const WeatherData& weather,
                        bool wifiOk,
                        bool timeSynced,
                        const char* ipStr) {
  if (!spriteReady) return;

  spr.fillSprite(COL_BG);

  // Outer dial ring & 12 hour ticks
  spr.drawCircle(120, 120, 116, COL_RING_DIM);
  for (int i = 0; i < 12; ++i) {
    int x1, y1, x2, y2;
    polarToXY(120, 120, 112, i * 30.0f, x1, y1);
    polarToXY(120, 120, 117, i * 30.0f, x2, y2);
    spr.drawLine(x1, y1, x2, y2, COL_MUTED);
  }

  struct tm timeinfo;
  bool hasTime = timeSynced && getLocalTime(&timeinfo, 10);

  if (hasTime) {
    // Seconds progress arc
    float secAngle = timeinfo.tm_sec * 6.0f;
    if (secAngle > 0.0f) {
      drawRingArc(120, 120, 116, 0.0f, secAngle, COL_CYAN, 3);
    }

    // Top date
    char dateBuf[24];
    strftime(dateBuf, sizeof(dateBuf), "%a %d %b", &timeinfo);
    for (int i = 0; dateBuf[i]; ++i) {
      dateBuf[i] = toupper(static_cast<unsigned char>(dateBuf[i]));
    }
    spr.setTextDatum(lgfx::middle_center);
    spr.setTextColor(COL_CLOUD, COL_BG);
    drawText(dateBuf, 120, 38, &fonts::Font2);

    // Main digital time
    char timeBuf[12];
    if (cfg.use24Hour) {
      snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
      spr.setTextColor(COL_WHITE, COL_BG);
      drawText(timeBuf, 120, 82, &fonts::Font6);
    } else {
      int hr12 = timeinfo.tm_hour % 12;
      if (hr12 == 0) hr12 = 12;
      snprintf(timeBuf, sizeof(timeBuf), "%d:%02d", hr12, timeinfo.tm_min);
      spr.setTextColor(COL_WHITE, COL_BG);
      drawText(timeBuf, 110, 82, &fonts::Font6);
      spr.setTextColor(COL_CYAN, COL_BG);
      drawText(timeinfo.tm_hour >= 12 ? "PM" : "AM", 192, 90, &fonts::Font2);
    }
  } else {
    spr.setTextDatum(lgfx::middle_center);
    spr.setTextColor(COL_MUTED, COL_BG);
    drawText("SYNCING TIME...", 120, 80, &fonts::Font2);
  }

  // Divider
  spr.drawFastHLine(44, 122, 152, COL_RING_DIM);

  // Bottom section: Weather or GPS prompt
  if (!cfg.locationConfigured) {
    spr.setTextDatum(lgfx::middle_center);
    spr.setTextColor(COL_AMBER, COL_BG);
    drawText("Set GPS in Web UI", 120, 154, &fonts::Font2);
    spr.setTextColor(COL_CYAN, COL_BG);
    drawText(ipStr ? ipStr : "flightclock.local", 120, 178, &fonts::Font2);
  } else if (weather.valid) {
    drawWeatherIcon(74, 162, weather.weatherCode, weather.isDay);

    float temp = cfg.useFahrenheit ? (weather.tempC * 9.0f / 5.0f + 32.0f) : weather.tempC;
    char tempBuf[16];
    snprintf(tempBuf, sizeof(tempBuf), "%.0f %c", temp, cfg.useFahrenheit ? 'F' : 'C');

    spr.setTextDatum(lgfx::middle_left);
    spr.setTextColor(COL_WHITE, COL_BG);
    drawText(tempBuf, 108, 152, &fonts::Font4);

    spr.setTextColor(COL_SKY, COL_BG);
    drawText(Weather::describeCode(weather.weatherCode), 108, 176, &fonts::Font2);

    float tMin = cfg.useFahrenheit ? (weather.tempMinC * 9.0f / 5.0f + 32.0f) : weather.tempMinC;
    float tMax = cfg.useFahrenheit ? (weather.tempMaxC * 9.0f / 5.0f + 32.0f) : weather.tempMaxC;
    char hiLoBuf[32];
    snprintf(hiLoBuf, sizeof(hiLoBuf), "H:%.0f  L:%.0f  RH:%d%%", tMax, tMin, weather.humidity);
    spr.setTextDatum(lgfx::middle_center);
    spr.setTextColor(COL_MUTED, COL_BG);
    drawText(hiLoBuf, 120, 202, &fonts::Font2);
  } else {
    spr.setTextDatum(lgfx::middle_center);
    spr.setTextColor(COL_MUTED, COL_BG);
    drawText("Fetching weather...", 120, 164, &fonts::Font2);
  }

  // Tiny Wi-Fi status dot at bottom
  spr.fillCircle(120, 224, 3, wifiOk ? COL_GREEN : COL_RED);

  spr.pushSprite(0, 0);
}

void renderFlightScreen(const AppConfig& cfg, const FlightInfo& flight) {
  if (!spriteReady) return;

  spr.fillSprite(COL_BG);

  // Outer radar ring & cardinal ticks
  spr.drawCircle(120, 120, 114, COL_RING_DIM);
  spr.drawCircle(120, 120, 110, COL_RING_DIM);
  for (int i = 0; i < 4; ++i) {
    int x1, y1, x2, y2;
    polarToXY(120, 120, 106, i * 90.0f, x1, y1);
    polarToXY(120, 120, 116, i * 90.0f, x2, y2);
    spr.drawLine(x1, y1, x2, y2, COL_MUTED);
  }

  // Bearing marker on outer radar ring
  int bx, by;
  polarToXY(120, 120, 112, flight.bearingDeg, bx, by);
  spr.fillCircle(bx, by, 6, COL_AMBER);
  spr.drawCircle(bx, by, 8, COL_WHITE);

  // Top badge
  spr.setTextDatum(lgfx::middle_center);
  spr.setTextColor(COL_AMBER, COL_BG);
  if (flight.totalInRange > 1) {
    char hdr[24];
    snprintf(hdr, sizeof(hdr), "OVERHEAD (1/%d)", flight.totalInRange);
    drawText(hdr, 120, 32, &fonts::Font2);
  } else {
    drawText("PLANE OVERHEAD", 120, 32, &fonts::Font2);
  }

  // Callsign
  spr.setTextColor(COL_WHITE, COL_BG);
  drawText(flight.callsign[0] ? flight.callsign : "UNKNOWN", 120, 62, &fonts::Font4);

  // Type & Registration
  char subBuf[32];
  if (flight.aircraftType[0] && flight.registration[0]) {
    snprintf(subBuf, sizeof(subBuf), "%s  %s", flight.aircraftType, flight.registration);
  } else if (flight.aircraftType[0]) {
    snprintf(subBuf, sizeof(subBuf), "Type: %s", flight.aircraftType);
  } else if (flight.registration[0]) {
    snprintf(subBuf, sizeof(subBuf), "Reg: %s", flight.registration);
  } else {
    snprintf(subBuf, sizeof(subBuf), "ADS-B Target");
  }
  spr.setTextColor(COL_SKY, COL_BG);
  drawText(subBuf, 120, 88, &fonts::Font2);

  spr.drawFastHLine(38, 106, 164, COL_RING_DIM);

  // Altitude
  char altNum[16], altBuf[24];
  if (cfg.useImperialDist) {
    formatThousands(flight.altitudeFt, altNum, sizeof(altNum));
    snprintf(altBuf, sizeof(altBuf), "%s ft", altNum);
  } else {
    int32_t altM = static_cast<int32_t>(roundf(flight.altitudeFt * 0.3048f));
    formatThousands(altM, altNum, sizeof(altNum));
    snprintf(altBuf, sizeof(altBuf), "%s m", altNum);
  }
  spr.setTextDatum(lgfx::middle_left);
  spr.setTextColor(COL_MUTED, COL_BG);
  drawText("ALT", 44, 124, &fonts::Font2);
  spr.setTextDatum(lgfx::middle_right);
  spr.setTextColor(COL_WHITE, COL_BG);
  drawText(altBuf, 196, 124, &fonts::Font2);

  // Vertical trend arrow
  if (flight.verticalRateFpm > 250) {
    spr.fillTriangle(124, 119, 119, 128, 129, 128, COL_GREEN);
  } else if (flight.verticalRateFpm < -250) {
    spr.fillTriangle(124, 129, 119, 120, 129, 120, COL_AMBER);
  }

  // Speed
  char spdBuf[24];
  if (cfg.useImperialDist) {
    snprintf(spdBuf, sizeof(spdBuf), "%.0f kt", flight.speedKnots);
  } else {
    snprintf(spdBuf, sizeof(spdBuf), "%.0f km/h", flight.speedKnots * 1.852f);
  }
  spr.setTextDatum(lgfx::middle_left);
  spr.setTextColor(COL_MUTED, COL_BG);
  drawText("SPD", 44, 148, &fonts::Font2);
  spr.setTextDatum(lgfx::middle_right);
  spr.setTextColor(COL_WHITE, COL_BG);
  drawText(spdBuf, 196, 148, &fonts::Font2);

  // Distance & Bearing
  char distBuf[28];
  if (cfg.useImperialDist) {
    snprintf(distBuf, sizeof(distBuf), "%.1f mi %s", flight.distanceKm * 0.621371f, cardinalDir(flight.bearingDeg));
  } else {
    snprintf(distBuf, sizeof(distBuf), "%.1f km %s", flight.distanceKm, cardinalDir(flight.bearingDeg));
  }
  spr.setTextDatum(lgfx::middle_left);
  spr.setTextColor(COL_MUTED, COL_BG);
  drawText("DIST", 44, 172, &fonts::Font2);
  spr.setTextDatum(lgfx::middle_right);
  spr.setTextColor(COL_CYAN, COL_BG);
  drawText(distBuf, 196, 172, &fonts::Font2);

  // Footer: Origin -> Destination
  spr.setTextDatum(lgfx::middle_center);
  if (flight.origin[0] && flight.destination[0]) {
    char routeBuf[24];
    snprintf(routeBuf, sizeof(routeBuf), "%s -> %s", flight.origin, flight.destination);
    spr.setTextColor(COL_AMBER, COL_BG);
    bool shortCodes = (strlen(flight.origin) <= 3 && strlen(flight.destination) <= 3);
    drawText(routeBuf, 120, 202, shortCodes ? &fonts::Font4 : &fonts::Font2);
  } else {
    spr.setTextColor(COL_MUTED, COL_BG);
    drawText("Route N/A", 120, 202, &fonts::Font2);
  }

  spr.pushSprite(0, 0);
}

} // namespace Display
