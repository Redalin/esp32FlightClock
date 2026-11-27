# ESP32-C3 Flight Clock & Weather Display

A compact round-screen widget for the **ESP32-C3 Super Mini** paired with a **1.28" 240×240 GC9A01 Round TFT** display.

- **Time & Weather Watchface**: NTP-synced clock (12h/24h, POSIX timezone with automatic DST), 60-tick precision dial, custom vector weather icons, and live weather from **Open-Meteo** (no API key required).
- **Overhead Flight Radar**: Automatically switches to a flight HUD whenever an aircraft enters your configured radius (default 5 km), using **airplanes.live** / **adsb.lol** with automatic fallback and `429`/`403` backoff. Shows callsign, rotated aircraft icon, type, registration, altitude with climb/descent indicator, ground speed, distance, route, and a bearing blip on the outer radar ring. Polls at your configured interval when idle (default 30s), speeds up to 5s while tracking an aircraft, and supports configurable **quiet hours** (default 1:00 AM–6:00 AM) to conserve API quota.
- **Web Configuration Portal**: Captive portal on first boot (`ESP32-FlightClock`) and always-on web UI in STA mode (`http://flightclock.local` or device IP) to configure Wi-Fi, GPS coordinates, timezone, units, brightness, radar radius/polling, and quiet hours.

> **New to ESP32?** See the step-by-step [First-Time User Guide](FIRST_TIME_USERS.md) for wiring, installing VS Code + PlatformIO (or flashing from the terminal), bootloader mode, and initial Wi-Fi setup.

---

## Wiring (ESP32-C3 Super Mini ↔ 1.28" GC9A01 240×240 TFT)

| Display Pin | ESP32-C3 Super Mini Pin | Notes |
| :--- | :--- | :--- |
| `VCC` | `3.3V` | Use 3.3V (not 5V) unless your module has a 3.3V regulator |
| `GND` | `GND` | Common ground |
| `SCL` / `SCK` | `GPIO 4` | Hardware SPI clock |
| `SDA` / `MOSI` | `GPIO 6` | Hardware SPI MOSI |
| `CS` | `GPIO 7` | Chip Select |
| `DC` | `GPIO 3` | Data / Command |
| `RST` | `GPIO 1` | Reset |
| `BLK` / `BL` | `GPIO 0` | Backlight PWM (or tie to `3.3V` for always-on) |

Pin mappings are configured in [include/config.h](include/config.h) using **LovyanGFX**.

---

## Build & Upload (PlatformIO)

1. Install [PlatformIO](https://platformio.org/) (VS Code extension or `pio` CLI).
2. *(Optional)* Create a `.env` file in the project root to pre-populate defaults at compile time (`WIFI_SSID`, `WIFI_PASS`, `TIMEZONE`, `LATITUDE`, `LONGITUDE`, `ADSB_LOL_API_KEY`).
3. Connect your ESP32-C3 Super Mini via USB-C (needed once to flash the initial OTA-enabled firmware and partition table).
4. Build and flash over USB:
   ```bash
   pio run -e esp32-c3-supermini -t upload
   ```
5. (Optional) Open serial monitor:
   ```bash
   pio device monitor -b 115200
   ```

### Over-The-Air (OTA) Firmware Updates

Once the device is connected to your Wi-Fi network, you can update the firmware wirelessly in either of two ways:

- **PlatformIO CLI (`espota`)**:
  ```bash
  pio run -e esp32-c3-supermini-ota -t upload
  ```
  By default this targets `flightclock.local`. You can optionally set `OTA_HOST` (an IP address, if mDNS isn't resolved on your network) and `OTA_PASSWORD` in `.env`.
- **Web UI Upload**:
  1. *(Optional)* Bump `FW_VERSION` in [include/config.h](include/config.h) for each release.
  2. Build the binary with `pio run -e esp32-c3-supermini` (outputs `.pio/build/esp32-c3-supermini/firmware_v<FW_VERSION>.bin`).
  3. Open **`http://flightclock.local`** (or the device's IP) in your browser.
  4. Under **Firmware Update (OTA)**, select `.pio/build/esp32-c3-supermini/firmware_v<FW_VERSION>.bin` and click **Upload Firmware**.

Both methods show a live circular progress ring on the round TFT screen and automatically reboot into the new firmware.

---

## First-Time Setup

1. On first boot (no Wi-Fi saved), the screen displays **SETUP MODE**.
2. Connect your phone or computer to the **`ESP32-FlightClock`** Wi-Fi network.
3. A captive portal should open automatically, or browse to **`http://192.168.4.1`**.
4. Enter your **Wi-Fi SSID & Password** (use **Scan** to list nearby 2.4 GHz networks), **GPS Latitude/Longitude** (or click **Use Browser GPS**), **Timezone**, and **Flight Radar** preferences, then click **Save & Apply**.
5. Once connected to your home Wi-Fi, you can revisit the web interface anytime at **`http://flightclock.local`** (or the IP shown in the Live Status panel) and use **Test Flight Screen** to preview the overhead flight card.
