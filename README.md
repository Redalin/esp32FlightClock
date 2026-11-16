# ESP32-C3 Flight Clock & Weather Display

A compact round-screen widget for the **ESP32-C3 Super Mini** paired with a **1.28" 240×240 GC9A01 Round TFT** display.

- **Time & Weather Watchface**: NTP-synced clock (12h/24h, POSIX timezone with automatic DST), seconds progress ring, custom vector weather icons, and live weather from **Open-Meteo** (no API key required).
- **Overhead Flight Radar**: Automatically switches to a flight card whenever an aircraft enters your configured radius (via **airplanes.live** or **adsb.lol**, no API key required), showing callsign, aircraft type, registration, altitude with climb/descent indicator, ground speed, distance, heading, and a bearing dot around the circular rim.
- **Web Configuration Portal**: Captive portal on first boot (`ESP32-FlightClock`) and always-on web UI in STA mode (`http://flightclock.local` or device IP) to configure Wi-Fi, GPS coordinates, timezone, units, brightness, and radar radius/polling.

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
2. Connect your ESP32-C3 Super Mini via USB-C.
3. Build and flash:
   ```bash
   pio run -t upload
   ```
4. (Optional) Open serial monitor:
   ```bash
   pio device monitor -b 115200
   ```

---

## First-Time Setup

1. On first boot (no Wi-Fi saved), the screen displays **SETUP MODE**.
2. Connect your phone or computer to the **`ESP32-FlightClock`** Wi-Fi network.
3. A captive portal should open automatically, or browse to **`http://192.168.4.1`**.
4. Enter your **Wi-Fi SSID & Password** (use **Scan** to list nearby 2.4 GHz networks), **GPS Latitude/Longitude** (or click **Use Browser GPS**), **Timezone**, and **Flight Radar** preferences, then click **Save & Apply**.
5. Once connected to your home Wi-Fi, you can revisit the web interface anytime at **`http://flightclock.local`** (or the IP shown in the Live Status panel) and use **Test Flight Screen** to preview the overhead flight card.
