#include "display.h"
#include <LovyanGFX.hpp>
#include <ctype.h>
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

// Palette tuned for clean 8-bit (RGB332) sprite quantization
constexpr uint16_t COL_BG         = 0x0000; // Pure black
constexpr uint16_t COL_PANEL      = 0x012A; // Deep midnight blue (0, 36, 85)
constexpr uint16_t COL_PANEL_EDGE = 0x224A; // Slate blue border (36, 73, 85)
constexpr uint16_t COL_RING_DIM   = 0x224A; // Dark slate ring
constexpr uint16_t COL_RADAR_DIM  = 0x024A; // Subtle radar teal-blue
constexpr uint16_t COL_CYAN       = 0x07FF; // Bright cyan
constexpr uint16_t COL_TEAL       = 0x04B5; // Muted teal
constexpr uint16_t COL_AMBER      = 0xFD20; // Warm amber / gold
constexpr uint16_t COL_SKY        = 0x5E5F; // Sky blue
constexpr uint16_t COL_SUN        = 0xFFE0; // Bright yellow
constexpr uint16_t COL_CLOUD      = 0xDEFB; // Bright silver-white
constexpr uint16_t COL_CLOUD_DIM  = 0x9CD3; // Soft cloud shadow
constexpr uint16_t COL_RAIN       = 0x3DDF; // Rain blue
constexpr uint16_t COL_MUTED      = 0x9492; // Mid neutral gray
constexpr uint16_t COL_WHITE      = 0xFFFF; // Pure white
constexpr uint16_t COL_GREEN      = 0x07E0; // Bright green
constexpr uint16_t COL_RED        = 0xF800; // Red

constexpr float kDegToRad = 0.0174532925f;
constexpr int kCx = 120;
constexpr int kCy = 120;

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

enum class ScreenMode : uint8_t {
  NONE = 0,
  BOOT,
  PORTAL,
  CLOCK,
  FLIGHT
};

ScreenMode activeScreen = ScreenMode::NONE;

void drawThickArc(int cx, int cy, float r, float startDeg, float endDeg,
                  uint16_t color, int thickness) {
  if (endDeg <= startDeg) return;
  int rIn = static_cast<int>(r) - thickness / 2;
  int rOut = rIn + thickness - 1;
  if (rIn < 0) rIn = 0;
  // LovyanGFX fillArc uses 0 deg = 3 o'clock, clockwise; our convention is 0 deg = 12 o'clock.
  spr.fillArc(cx, cy, rOut, rIn, startDeg + 270.0f, endDeg + 270.0f, color);
}

void drawDialTicks(int cx, int cy, float innerR, float outerR,
                   uint16_t majorCol, uint16_t minorCol) {
  struct TickCoord {
    int16_t x1, y1, x2, y2;
    int16_t x1b, y1b, x2b, y2b;
    uint8_t kind; // 0 = minute dot, 1 = hour tick, 2 = cardinal tick
  };
  static TickCoord ticks[60];
  static bool ticksReady = false;

  if (!ticksReady) {
    for (int i = 0; i < 60; ++i) {
      float deg = i * 6.0f;
      bool isCardinal = (i % 15 == 0);
      bool isHour = (i % 5 == 0);
      if (!isHour) {
        int mx, my;
        polarToXY(cx, cy, outerR - 1.0f, deg, mx, my);
        ticks[i].x1 = static_cast<int16_t>(mx);
        ticks[i].y1 = static_cast<int16_t>(my);
        ticks[i].kind = 0;
        continue;
      }
      float r1 = isCardinal ? (innerR - 3.0f) : innerR;
      int x1, y1, x2, y2;
      polarToXY(cx, cy, r1, deg, x1, y1);
      polarToXY(cx, cy, outerR, deg, x2, y2);
      ticks[i].x1 = static_cast<int16_t>(x1);
      ticks[i].y1 = static_cast<int16_t>(y1);
      ticks[i].x2 = static_cast<int16_t>(x2);
      ticks[i].y2 = static_cast<int16_t>(y2);
      ticks[i].kind = isCardinal ? 2 : 1;
      if (isCardinal) {
        int x1b, y1b, x2b, y2b;
        polarToXY(cx, cy, r1, deg + 0.6f, x1b, y1b);
        polarToXY(cx, cy, outerR, deg + 0.6f, x2b, y2b);
        ticks[i].x1b = static_cast<int16_t>(x1b);
        ticks[i].y1b = static_cast<int16_t>(y1b);
        ticks[i].x2b = static_cast<int16_t>(x2b);
        ticks[i].y2b = static_cast<int16_t>(y2b);
      }
    }
    ticksReady = true;
  }

  for (int i = 0; i < 60; ++i) {
    const TickCoord& t = ticks[i];
    if (t.kind == 0) {
      spr.drawPixel(t.x1, t.y1, minorCol);
    } else if (t.kind == 1) {
      spr.drawLine(t.x1, t.y1, t.x2, t.y2, minorCol);
    } else {
      spr.drawLine(t.x1, t.y1, t.x2, t.y2, majorCol);
      spr.drawLine(t.x1b, t.y1b, t.x2b, t.y2b, majorCol);
    }
  }
}

// Vector airplane silhouette pointing along angleDeg (0 = up / 12 o'clock)
void drawPlaneIcon(int cx, int cy, float scale, float angleDeg, uint16_t color) {
  auto pt = [&](float lx, float ly, int& ox, int& oy) {
    float rad = angleDeg * kDegToRad;
    float s = sinf(rad);
    float c = cosf(rad);
    float rx = lx * c - ly * s;
    float ry = lx * s + ly * c;
    ox = cx + static_cast<int>(roundf(rx * scale));
    oy = cy + static_cast<int>(roundf(ry * scale));
  };

  int nX, nY, tX, tY;
  pt(0.0f, -10.0f, nX, nY);
  pt(0.0f, 9.0f, tX, tY);

  // Main swept wings
  int lwX, lwY, rwX, rwY, wcX, wcY, wfX, wfY;
  pt(-9.5f, 2.5f, lwX, lwY);
  pt(9.5f, 2.5f, rwX, rwY);
  pt(0.0f, -3.5f, wfX, wfY);
  pt(0.0f, 1.5f, wcX, wcY);
  spr.fillTriangle(wfX, wfY, lwX, lwY, wcX, wcY, color);
  spr.fillTriangle(wfX, wfY, rwX, rwY, wcX, wcY, color);

  // Tailplane
  int ltX, ltY, rtX, rtY, tcX, tcY;
  pt(-4.5f, 8.5f, ltX, ltY);
  pt(4.5f, 8.5f, rtX, rtY);
  pt(0.0f, 5.5f, tcX, tcY);
  spr.fillTriangle(tcX, tcY, ltX, ltY, tX, tY, color);
  spr.fillTriangle(tcX, tcY, rtX, rtY, tX, tY, color);

  // Fuselage
  int fL1, fL2, fR1, fR2;
  pt(-1.3f, -4.0f, fL1, fL2);
  pt(1.3f, -4.0f, fR1, fR2);
  spr.fillTriangle(nX, nY, fL1, fL2, fR1, fR2, color);
  spr.drawLine(nX, nY, tX, tY, color);
  int c1x, c1y, c2x, c2y;
  pt(-0.8f, 0.0f, c1x, c1y);
  pt(0.8f, 0.0f, c2x, c2y);
  spr.drawLine(nX, nY, c1x, c1y, color);
  spr.drawLine(nX, nY, c2x, c2y, color);
  spr.drawLine(c1x, c1y, tX, tY, color);
  spr.drawLine(c2x, c2y, tX, tY, color);
}

void drawSun(int cx, int cy, int r) {
  spr.fillCircle(cx, cy, r + 2, COL_AMBER);
  spr.fillCircle(cx, cy, r, COL_SUN);
  for (int i = 0; i < 8; ++i) {
    float a = i * 45.0f;
    int x1, y1, x2, y2;
    polarToXY(cx, cy, r + 4, a, x1, y1);
    polarToXY(cx, cy, r + 7, a, x2, y2);
    spr.drawLine(x1, y1, x2, y2, COL_SUN);
  }
}

void drawMoon(int cx, int cy, int r, uint16_t bgColor) {
  spr.fillCircle(cx, cy, r, COL_SUN);
  spr.fillCircle(cx + r / 2, cy - r / 4, r - 2, bgColor);
}

void drawCloud(int cx, int cy, uint16_t topCol, uint16_t baseCol) {
  spr.fillCircle(cx - 8, cy + 2, 7, baseCol);
  spr.fillCircle(cx + 9, cy + 2, 7, baseCol);
  spr.fillRoundRect(cx - 15, cy + 2, 31, 8, 4, baseCol);

  spr.fillCircle(cx - 7, cy + 1, 7, topCol);
  spr.fillCircle(cx + 1, cy - 4, 9, topCol);
  spr.fillCircle(cx + 9, cy + 1, 6, topCol);
  spr.fillRoundRect(cx - 14, cy + 1, 29, 7, 3, topCol);
}

void drawWeatherIcon(int cx, int cy, int code, bool isDay, uint16_t bgColor) {
  if (code == 0 || code == 1) {
    if (isDay) drawSun(cx, cy, 9);
    else drawMoon(cx, cy, 10, bgColor);
    return;
  }
  if (code == 2) {
    if (isDay) drawSun(cx - 7, cy - 6, 6);
    else drawMoon(cx - 7, cy - 6, 6, bgColor);
    drawCloud(cx + 3, cy + 4, COL_CLOUD, COL_CLOUD_DIM);
    return;
  }
  if (code == 3) {
    drawCloud(cx, cy, COL_CLOUD, COL_CLOUD_DIM);
    return;
  }
  if (code == 45 || code == 48) {
    drawCloud(cx, cy - 4, COL_CLOUD_DIM, COL_MUTED);
    spr.drawFastHLine(cx - 13, cy + 9, 26, COL_CLOUD);
    spr.drawFastHLine(cx - 9, cy + 13, 18, COL_MUTED);
    return;
  }
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
    drawCloud(cx, cy - 5, COL_CLOUD, COL_CLOUD_DIM);
    for (int dx = -8; dx <= 8; dx += 8) {
      spr.drawLine(cx + dx, cy + 7, cx + dx - 3, cy + 14, COL_RAIN);
      spr.drawLine(cx + dx + 1, cy + 7, cx + dx - 2, cy + 14, COL_SKY);
    }
    return;
  }
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) {
    drawCloud(cx, cy - 5, COL_CLOUD, COL_CLOUD_DIM);
    const int sx[3] = {-8, 0, 8};
    const int sy[3] = {10, 14, 10};
    for (int i = 0; i < 3; ++i) {
      spr.drawFastHLine(cx + sx[i] - 2, cy + sy[i], 5, COL_WHITE);
      spr.drawFastVLine(cx + sx[i], cy + sy[i] - 2, 5, COL_WHITE);
    }
    return;
  }
  if (code >= 95) {
    drawCloud(cx, cy - 5, COL_CLOUD_DIM, COL_MUTED);
    spr.fillTriangle(cx + 2, cy + 5, cx - 4, cy + 13, cx + 1, cy + 13, COL_SUN);
    spr.fillTriangle(cx + 1, cy + 11, cx - 2, cy + 19, cx + 5, cy + 11, COL_SUN);
    return;
  }
  drawCloud(cx, cy, COL_CLOUD, COL_CLOUD_DIM);
}

void drawWifiIcon(int cx, int cy, bool ok) {
  uint16_t col = ok ? COL_GREEN : COL_RED;
  spr.fillCircle(cx, cy + 3, 2, col);
  drawThickArc(cx, cy + 3, 6.0f, -45.0f, 45.0f, col, 1);
  drawThickArc(cx, cy + 3, 10.0f, -45.0f, 45.0f, ok ? COL_TEAL : COL_RED, 1);
  if (!ok) {
    spr.drawLine(cx - 6, cy - 5, cx + 6, cy + 5, COL_RED);
  }
}

void drawTempWithDegree(int x, int y, float temp, bool useFahrenheit, uint16_t color,
                        uint16_t bgColor, const lgfx::IFont* font) {
  char numBuf[12];
  snprintf(numBuf, sizeof(numBuf), "%.0f", temp);
  spr.setFont(font);
  spr.setTextDatum(lgfx::middle_left);
  spr.setTextColor(color, bgColor);
  spr.drawString(numBuf, x, y);

  int numW = spr.textWidth(numBuf);
  int degX = x + numW + 4;
  int degY = y - 6;
  spr.drawCircle(degX, degY, 3, color);

  char unitBuf[2] = {useFahrenheit ? 'F' : 'C', '\0'};
  spr.drawString(unitBuf, degX + 6, y);
}

const char* cardinalDir(float deg) {
  static const char* dirs[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  int idx = static_cast<int>(roundf(deg / 45.0f)) % 8;
  if (idx < 0) idx += 8;
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
  activeScreen = ScreenMode::BOOT;
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
  spr.drawCircle(kCx, kCy, 115, COL_RING_DIM);
  spr.drawCircle(kCx, kCy, 110, COL_TEAL);
  spr.drawCircle(kCx, kCy, 82, COL_RADAR_DIM);

  drawPlaneIcon(kCx, 72, 1.4f, 45.0f, COL_AMBER);

  spr.setTextDatum(lgfx::middle_center);
  spr.setTextColor(COL_WHITE, COL_BG);
  drawText("FLIGHT CLOCK", kCx, 114, &fonts::Font4);

  spr.fillRoundRect(46, 140, 148, 26, 13, COL_PANEL);
  spr.drawRoundRect(46, 140, 148, 26, 13, COL_PANEL_EDGE);
  spr.setTextColor(COL_CYAN, COL_PANEL);
  drawText(statusLine, kCx, 153, &fonts::Font2);

  spr.pushSprite(0, 0);
}

void showPortalScreen(const char* apSsid, const char* apIp) {
  if (!spriteReady) return;
  activeScreen = ScreenMode::PORTAL;

  spr.fillSprite(COL_BG);
  spr.drawCircle(kCx, kCy, 115, COL_AMBER);
  spr.drawCircle(kCx, kCy, 112, COL_PANEL_EDGE);

  spr.fillRoundRect(58, 32, 124, 24, 12, COL_AMBER);
  spr.setTextDatum(lgfx::middle_center);
  spr.setTextColor(COL_BG, COL_AMBER);
  drawText("SETUP MODE", kCx, 44, &fonts::Font2);

  // Step 1 card
  spr.fillRoundRect(28, 70, 184, 54, 10, COL_PANEL);
  spr.drawRoundRect(28, 70, 184, 54, 10, COL_PANEL_EDGE);
  spr.setTextColor(COL_MUTED, COL_PANEL);
  drawText("1. JOIN WI-FI", kCx, 84, &fonts::Font2);
  spr.setTextColor(COL_WHITE, COL_PANEL);
  drawText(apSsid, kCx, 106, &fonts::Font2);

  // Step 2 card
  spr.fillRoundRect(28, 134, 184, 56, 10, COL_PANEL);
  spr.drawRoundRect(28, 134, 184, 56, 10, COL_PANEL_EDGE);
  spr.setTextColor(COL_MUTED, COL_PANEL);
  drawText("2. OPEN BROWSER", kCx, 148, &fonts::Font2);
  spr.setTextColor(COL_CYAN, COL_PANEL);
  drawText(apIp, kCx, 171, &fonts::Font4);

  spr.pushSprite(0, 0);
}

void renderClockWeather(const AppConfig& cfg,
                        const WeatherData& weather,
                        bool wifiOk,
                        bool timeSynced,
                        const char* ipStr) {
  if (!spriteReady) return;

  struct tm timeinfo = {};
  bool hasTime = timeSynced && getLocalTime(&timeinfo, 10);

  static int lastMin = -1;
  static int lastHour = -1;
  static int lastMday = -1;
  static bool lastHasTime = false;
  static bool lastWifiOk = false;
  static bool lastLocConfigured = false;
  static bool lastUse24h = true;
  static bool lastUseF = false;
  static bool lastWeatherValid = false;
  static unsigned long lastWeatherUpdatedMs = 0;

  bool staticStateSame = (activeScreen == ScreenMode::CLOCK) &&
                         (hasTime == lastHasTime) &&
                         (wifiOk == lastWifiOk) &&
                         (cfg.locationConfigured == lastLocConfigured) &&
                         (cfg.use24Hour == lastUse24h) &&
                         (cfg.useFahrenheit == lastUseF) &&
                         (weather.valid == lastWeatherValid) &&
                         (weather.updatedAtMs == lastWeatherUpdatedMs);

  if (staticStateSame &&
      (!hasTime || (timeinfo.tm_min == lastMin &&
                    timeinfo.tm_hour == lastHour &&
                    timeinfo.tm_mday == lastMday))) {
    return;
  }

  activeScreen = ScreenMode::CLOCK;
  lastHasTime = hasTime;
  lastWifiOk = wifiOk;
  lastLocConfigured = cfg.locationConfigured;
  lastUse24h = cfg.use24Hour;
  lastUseF = cfg.useFahrenheit;
  lastWeatherValid = weather.valid;
  lastWeatherUpdatedMs = weather.updatedAtMs;
  if (hasTime) {
    lastMin = timeinfo.tm_min;
    lastHour = timeinfo.tm_hour;
    lastMday = timeinfo.tm_mday;
  }

  spr.fillSprite(COL_BG);

  // Outer precision bezel & ticks
  spr.drawCircle(kCx, kCy, 116, COL_RING_DIM);
  drawDialTicks(kCx, kCy, 110.0f, 117.0f, COL_TEAL, COL_RING_DIM);

  if (hasTime) {
    // Top date pill
    char dateBuf[24];
    strftime(dateBuf, sizeof(dateBuf), "%a %d %b", &timeinfo);
    for (int i = 0; dateBuf[i]; ++i) {
      dateBuf[i] = toupper(static_cast<unsigned char>(dateBuf[i]));
    }
    spr.fillRoundRect(68, 25, 104, 20, 10, COL_PANEL);
    spr.setTextDatum(lgfx::middle_center);
    spr.setTextColor(COL_SKY, COL_PANEL);
    drawText(dateBuf, kCx, 35, &fonts::Font2);

    // Main digital time
    char timeBuf[12];
    if (cfg.use24Hour) {
      snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
      spr.setTextColor(COL_WHITE, COL_BG);
      drawText(timeBuf, kCx, 80, &fonts::Font6);
    } else {
      int hr12 = timeinfo.tm_hour % 12;
      if (hr12 == 0) hr12 = 12;
      snprintf(timeBuf, sizeof(timeBuf), "%d:%02d", hr12, timeinfo.tm_min);
      spr.setTextColor(COL_WHITE, COL_BG);
      drawText(timeBuf, 104, 80, &fonts::Font6);

      spr.fillRoundRect(174, 72, 30, 18, 5, COL_PANEL);
      spr.drawRoundRect(174, 72, 30, 18, 5, COL_TEAL);
      spr.setTextColor(COL_CYAN, COL_PANEL);
      drawText(timeinfo.tm_hour >= 12 ? "PM" : "AM", 189, 81, &fonts::Font2);
    }
  } else {
    spr.setTextDatum(lgfx::middle_center);
    spr.setTextColor(COL_MUTED, COL_BG);
    drawText("SYNCING TIME...", kCx, 78, &fonts::Font2);
  }

  // Lower Weather Card
  spr.fillRoundRect(26, 116, 188, 92, 14, COL_PANEL);
  spr.drawRoundRect(26, 116, 188, 92, 14, COL_PANEL_EDGE);

  if (!cfg.locationConfigured) {
    spr.setTextDatum(lgfx::middle_center);
    spr.setTextColor(COL_AMBER, COL_PANEL);
    drawText("Set GPS in Web UI", kCx, 148, &fonts::Font2);
    spr.setTextColor(COL_CYAN, COL_PANEL);
    drawText(ipStr && ipStr[0] ? ipStr : "flightclock.local", kCx, 174, &fonts::Font2);
  } else if (weather.valid) {
    drawWeatherIcon(64, 152, weather.weatherCode, weather.isDay, COL_PANEL);

    float temp = cfg.useFahrenheit ? (weather.tempC * 9.0f / 5.0f + 32.0f) : weather.tempC;
    drawTempWithDegree(100, 140, temp, cfg.useFahrenheit, COL_WHITE, COL_PANEL, &fonts::Font4);

    spr.setTextDatum(lgfx::middle_left);
    spr.setTextColor(COL_SKY, COL_PANEL);
    drawText(Weather::describeCode(weather.weatherCode), 100, 166, &fonts::Font2);

    spr.drawFastHLine(40, 181, 160, COL_PANEL_EDGE);

    float tMin = cfg.useFahrenheit ? (weather.tempMinC * 9.0f / 5.0f + 32.0f) : weather.tempMinC;
    float tMax = cfg.useFahrenheit ? (weather.tempMaxC * 9.0f / 5.0f + 32.0f) : weather.tempMaxC;
    char hiLoBuf[32];
    snprintf(hiLoBuf, sizeof(hiLoBuf), "H:%.0f  L:%.0f  RH:%d%%", tMax, tMin, weather.humidity);
    spr.setTextDatum(lgfx::middle_center);
    spr.setTextColor(COL_MUTED, COL_PANEL);
    drawText(hiLoBuf, kCx, 195, &fonts::Font2);
  } else {
    spr.setTextDatum(lgfx::middle_center);
    spr.setTextColor(COL_MUTED, COL_PANEL);
    drawText("Fetching weather...", kCx, 162, &fonts::Font2);
  }

  // Bottom Wi-Fi icon
  drawWifiIcon(kCx, 222, wifiOk);

  spr.pushSprite(0, 0);
}

void renderFlightScreen(const AppConfig& cfg, const FlightInfo& flight) {
  if (!spriteReady) return;

  static unsigned long lastSeenMs = 0;
  static int32_t lastAltFt = 0;
  static int32_t lastDistTenths = -1;
  static int32_t lastBearingDeg = -1;
  static int lastTotalInRange = -1;
  static bool lastImperial = false;
  static char lastCallsign[16] = {0};

  int32_t distTenths = static_cast<int32_t>(roundf(flight.distanceKm * 10.0f));
  int32_t bearingInt = static_cast<int32_t>(roundf(flight.bearingDeg));
  if (activeScreen == ScreenMode::FLIGHT &&
      lastSeenMs == flight.lastSeenMs &&
      lastAltFt == flight.altitudeFt &&
      lastDistTenths == distTenths &&
      lastBearingDeg == bearingInt &&
      lastTotalInRange == flight.totalInRange &&
      lastImperial == cfg.useImperialDist &&
      strncmp(lastCallsign, flight.callsign, sizeof(lastCallsign)) == 0) {
    return;
  }

  activeScreen = ScreenMode::FLIGHT;
  lastSeenMs = flight.lastSeenMs;
  lastAltFt = flight.altitudeFt;
  lastDistTenths = distTenths;
  lastBearingDeg = bearingInt;
  lastTotalInRange = flight.totalInRange;
  lastImperial = cfg.useImperialDist;
  strlcpy(lastCallsign, flight.callsign, sizeof(lastCallsign));

  spr.fillSprite(COL_BG);

  // Concentric radar rings
  spr.drawCircle(kCx, kCy, 115, COL_RADAR_DIM);
  spr.drawCircle(kCx, kCy, 111, COL_RING_DIM);
  spr.drawCircle(kCx, kCy, 80, COL_RADAR_DIM);

  // Cardinal compass ticks & tiny N/E/S/W markers
  const char* cardLabels[4] = {"N", "E", "S", "W"};
  spr.setTextDatum(lgfx::middle_center);
  spr.setTextColor(COL_TEAL, COL_BG);
  for (int i = 0; i < 4; ++i) {
    float deg = i * 90.0f;
    int x1, y1, x2, y2, lx, ly;
    polarToXY(kCx, kCy, 108.0f, deg, x1, y1);
    polarToXY(kCx, kCy, 116.0f, deg, x2, y2);
    spr.drawLine(x1, y1, x2, y2, COL_TEAL);
    polarToXY(kCx, kCy, 101.0f, deg, lx, ly);
    drawText(cardLabels[i], lx, ly, &fonts::Font0);
  }

  // Bearing target blip on outer radar ring
  int bx, by;
  polarToXY(kCx, kCy, 113.0f, flight.bearingDeg, bx, by);
  spr.drawCircle(bx, by, 8, COL_AMBER);
  spr.fillCircle(bx, by, 5, COL_AMBER);
  spr.fillCircle(bx, by, 2, COL_WHITE);

  // Top status pill
  spr.fillRoundRect(56, 20, 128, 20, 10, COL_AMBER);
  spr.setTextDatum(lgfx::middle_center);
  spr.setTextColor(COL_BG, COL_AMBER);
  if (flight.totalInRange > 1) {
    char hdr[24];
    snprintf(hdr, sizeof(hdr), "OVERHEAD (1/%d)", flight.totalInRange);
    drawText(hdr, kCx, 30, &fonts::Font2);
  } else {
    drawText("PLANE OVERHEAD", kCx, 30, &fonts::Font2);
  }

  // Callsign + rotated aircraft heading icon
  const char* callsign = flight.callsign[0] ? flight.callsign : "UNKNOWN";
  spr.setFont(&fonts::Font4);
  int csW = spr.textWidth(callsign);
  int planeX = kCx - csW / 2 - 14;
  if (planeX > 36) {
    drawPlaneIcon(planeX, 58, 0.9f, flight.trackDeg, COL_CYAN);
  }
  spr.setTextColor(COL_WHITE, COL_BG);
  drawText(callsign, (planeX > 36) ? (kCx + 8) : kCx, 58, &fonts::Font4);

  // Type & Registration
  char subBuf[32];
  if (flight.aircraftType[0] && flight.registration[0]) {
    snprintf(subBuf, sizeof(subBuf), "%s  -  %s", flight.aircraftType, flight.registration);
  } else if (flight.aircraftType[0]) {
    snprintf(subBuf, sizeof(subBuf), "Type: %s", flight.aircraftType);
  } else if (flight.registration[0]) {
    snprintf(subBuf, sizeof(subBuf), "Reg: %s", flight.registration);
  } else {
    snprintf(subBuf, sizeof(subBuf), "ADS-B Target");
  }
  spr.setTextColor(COL_SKY, COL_BG);
  drawText(subBuf, kCx, 82, &fonts::Font2);

  // Telemetry HUD panel
  spr.fillRoundRect(30, 98, 180, 82, 12, COL_PANEL);
  spr.drawRoundRect(30, 98, 180, 82, 12, COL_PANEL_EDGE);
  spr.drawFastHLine(40, 125, 160, COL_PANEL_EDGE);
  spr.drawFastHLine(40, 152, 160, COL_PANEL_EDGE);

  // Altitude row
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
  spr.setTextColor(COL_MUTED, COL_PANEL);
  drawText("ALT", 42, 112, &fonts::Font2);
  spr.setTextDatum(lgfx::middle_right);
  spr.setTextColor(COL_WHITE, COL_PANEL);
  drawText(altBuf, 198, 112, &fonts::Font2);

  // Vertical trend arrow
  if (flight.verticalRateFpm > 250) {
    spr.fillTriangle(116, 107, 111, 116, 121, 116, COL_GREEN);
  } else if (flight.verticalRateFpm < -250) {
    spr.fillTriangle(116, 117, 111, 108, 121, 108, COL_AMBER);
  }

  // Speed row
  char spdBuf[24];
  if (cfg.useImperialDist) {
    snprintf(spdBuf, sizeof(spdBuf), "%.0f kt", flight.speedKnots);
  } else {
    snprintf(spdBuf, sizeof(spdBuf), "%.0f km/h", flight.speedKnots * 1.852f);
  }
  spr.setTextDatum(lgfx::middle_left);
  spr.setTextColor(COL_MUTED, COL_PANEL);
  drawText("SPD", 42, 139, &fonts::Font2);
  spr.setTextDatum(lgfx::middle_right);
  spr.setTextColor(COL_WHITE, COL_PANEL);
  drawText(spdBuf, 198, 139, &fonts::Font2);

  // Distance & Bearing row
  char distBuf[28];
  if (cfg.useImperialDist) {
    snprintf(distBuf, sizeof(distBuf), "%.1f mi %s",
             flight.distanceKm * 0.621371f, cardinalDir(flight.bearingDeg));
  } else {
    snprintf(distBuf, sizeof(distBuf), "%.1f km %s",
             flight.distanceKm, cardinalDir(flight.bearingDeg));
  }
  spr.setTextDatum(lgfx::middle_left);
  spr.setTextColor(COL_MUTED, COL_PANEL);
  drawText("DIST", 42, 166, &fonts::Font2);
  spr.setTextDatum(lgfx::middle_right);
  spr.setTextColor(COL_CYAN, COL_PANEL);
  drawText(distBuf, 198, 166, &fonts::Font2);

  // Footer: Origin [plane] Destination
  spr.setTextDatum(lgfx::middle_center);
  if (flight.origin[0] && flight.destination[0]) {
    bool shortCodes = (strlen(flight.origin) <= 3 && strlen(flight.destination) <= 3);
    const lgfx::IFont* routeFont = shortCodes ? &fonts::Font4 : &fonts::Font2;
    int offset = shortCodes ? 36 : 42;
    spr.setTextColor(COL_AMBER, COL_BG);
    drawText(flight.origin, kCx - offset, 200, routeFont);
    drawPlaneIcon(kCx, 200, 0.85f, 90.0f, COL_WHITE);
    drawText(flight.destination, kCx + offset, 200, routeFont);
  } else {
    spr.setTextColor(COL_MUTED, COL_BG);
    drawText("Route N/A", kCx, 200, &fonts::Font2);
  }

  spr.pushSprite(0, 0);
}

} // namespace Display
