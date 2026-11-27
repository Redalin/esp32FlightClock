# First-Time User Guide

This guide walks you through setting up and flashing the **ESP32-C3 Flight Clock & Weather Display** on a brand-new ESP32-C3 Super Mini from scratch — no prior ESP32 experience required.

---

## What You Need

- **ESP32-C3 Super Mini** board
- **1.28" 240×240 GC9A01 Round SPI TFT** display
- **8 jumper wires** (female-to-female or soldered connections)
- **A data-capable USB-C cable** *(many cheap USB-C cables are charge-only and will not show up on your computer)*
- **A 2.4 GHz Wi-Fi network** *(the ESP32-C3 does not support 5 GHz Wi-Fi)*

---

## Step 1: Wire the Display

Connect the GC9A01 display to the ESP32-C3 Super Mini while the board is **unplugged**:

| Display Pin | ESP32-C3 Super Mini Pin | Notes |
| :--- | :--- | :--- |
| `VCC` | `3.3V` | Use `3.3V` (not `5V`) unless your display explicitly has a 3.3V regulator |
| `GND` | `GND` | Common ground |
| `SCL` / `SCK` | `GPIO 4` | Hardware SPI clock |
| `SDA` / `MOSI` | `GPIO 6` | Hardware SPI data |
| `CS` | `GPIO 7` | Chip Select |
| `DC` | `GPIO 3` | Data / Command |
| `RST` | `GPIO 1` | Reset |
| `BLK` / `BL` | `GPIO 0` | Backlight PWM (or connect to `3.3V` for always-on) |

---

## Step 2: Download This Project

Either clone the repository with Git:

```bash
git clone <repository-url>
cd esp32ScreenWidget
```

Or click **Code → Download ZIP** on GitHub and extract the folder somewhere easy to find.

---

## Step 3: Flash the Firmware to Your ESP32-C3

You only need to flash over USB **once**. After the first flash, future updates can be installed wirelessly over Wi-Fi (OTA).

Choose **one** of the two methods below:

### Option A: VS Code + PlatformIO (Recommended for Beginners)

1. **Install VS Code**: Download and install [Visual Studio Code](https://code.visualstudio.com/).
2. **Install the PlatformIO Extension**:
   - Open VS Code and click the **Extensions** icon on the left sidebar (`Ctrl+Shift+X` on Windows/Linux, `Cmd+Shift+X` on macOS).
   - Search for **`PlatformIO IDE`** (by PlatformIO) and click **Install**.
   - Wait for the installation to finish and reload VS Code if prompted.
3. **Open the Project**:
   - Click **File → Open Folder...** and select the `esp32ScreenWidget` folder.
   - Wait a minute or two while PlatformIO automatically downloads the ESP32-C3 toolchain and libraries (`LovyanGFX` and `ArduinoJson`).
4. **Connect the ESP32-C3**:
   - Plug the ESP32-C3 Super Mini into your computer with a data-capable USB-C cable.
   - *Drivers*: Windows 10/11, macOS, and Linux include built-in drivers for the ESP32-C3's USB serial interface.
   - *Linux permissions*: If you get a "Permission denied" error on `/dev/ttyACM0`, run `sudo usermod -aG dialout $USER`, then log out and log back in.
5. **Select the USB Environment**:
   - Look at the bottom blue status bar in VS Code. Make sure the active environment is **`env:esp32-c3-supermini`** (not `esp32-c3-supermini-ota`). Click the environment name in the status bar to switch if needed.
6. **Build & Upload**:
   - Click the **Upload** (`→`) button on the bottom status bar (or open the alien-head **PlatformIO** sidebar → `esp32-c3-supermini` → **General → Upload**).
   - Wait for `SUCCESS` in the terminal.
7. **Press `RST`** on the ESP32-C3 board once if it doesn't reboot automatically.
8. *(Optional)* Click the **Serial Monitor** (plug icon on the bottom bar) to view live startup logs at `115200` baud.

---

### Option B: Terminal / Command-Line Flasher (No VS Code Required)

#### B1. Using PlatformIO CLI (Builds & Flashes from Source)

1. Make sure **Python 3** is installed (`python3 --version`).
2. Install PlatformIO Core:
   ```bash
   pip install -U platformio
   ```
   *(On macOS with Homebrew, you can also run `brew install platformio`.)*
3. Plug in your ESP32-C3 via USB-C and run from inside the `esp32ScreenWidget` folder:
   ```bash
   pio run -e esp32-c3-supermini -t upload
   ```
4. *(Optional)* Watch serial output:
   ```bash
   pio device monitor -b 115200
   ```

#### B2. Using `esptool` (Flashing Pre-Built `.bin` Files)

If you already have compiled binary files (for example, from a release or `.pio/build/esp32-c3-supermini/`):

1. Install `esptool`:
   ```bash
   pip install esptool
   ```
2. Find your serial port:
   - **Windows**: Open *Device Manager → Ports (COM & LPT)* (e.g., `COM3`)
   - **macOS**: Run `ls /dev/cu.usbmodem*`
   - **Linux**: Run `ls /dev/ttyACM*`
3. Flash the board:
   - **If you have a single merged factory image** (`merged_firmware.bin`):
     ```bash
     esptool.py --chip esp32c3 --port <YOUR_PORT> --baud 460800 write_flash -z 0x0 merged_firmware.bin
     ```
   - **If you have separate PlatformIO build files** (brand-new boards need the bootloader, partition table, `boot_app0.bin`, and firmware):
     ```bash
     esptool.py --chip esp32c3 --port <YOUR_PORT> --baud 460800 write_flash -z \
       0x0 .pio/build/esp32-c3-supermini/bootloader.bin \
       0x8000 .pio/build/esp32-c3-supermini/partitions.bin \
       0xe000 ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin \
       0x10000 .pio/build/esp32-c3-supermini/firmware_v1.0.9.bin
     ```
     *(Replace `firmware_v1.0.9.bin` with your actual firmware version filename.)*
4. Press the **RST** button on the ESP32-C3 to start the new firmware.

---

### Putting a Brand-New ESP32-C3 Into Bootloader Mode

If uploading fails to connect (very common on a brand-new ESP32-C3 Super Mini):

1. **Hold** the **`BOOT`** button on the ESP32-C3.
2. While holding `BOOT`, **tap** the **`RST`** button (or plug in the USB-C cable).
3. **Release** the **`BOOT`** button.
4. Run the upload again, then tap **`RST`** once flashing completes.

---

## Step 4: Connect to Wi-Fi & Configure

Once flashed, the screen will light up and display **SETUP MODE**:

1. On your phone or computer, connect to the Wi-Fi network named **`ESP32-FlightClock`** (no password).
2. A configuration page should open automatically. If it doesn't, open a web browser and go to **`http://192.168.4.1`**.
3. Fill in your settings:
   - **Wi-Fi SSID & Password**: Click **Scan** to select your 2.4 GHz home Wi-Fi network and enter its password.
   - **GPS Location**: Click **Use Browser GPS** or paste your latitude & longitude.
   - **Timezone**: Pick your timezone so the clock and automatic DST are accurate.
   - **Display & Flight Radar**: Adjust brightness, units, search radius, max altitude, or quiet hours as desired.
4. Click **Save & Apply**.

The ESP32-C3 will reboot, join your home Wi-Fi, sync the time via NTP, fetch local weather, and begin watching for overhead aircraft.

After setup, you can open **`http://flightclock.local`** (or the device's IP address) from any device on your Wi-Fi to change settings, preview the flight radar with **Test Flight Screen**, or upload future firmware updates wirelessly (**Firmware Update (OTA)**).

---

## Troubleshooting

- **Computer doesn't detect the board / no COM or ttyACM port**:
  - Try a different USB-C cable — many cables only carry power and lack data wires.
  - Put the board into bootloader mode (hold **BOOT**, plug in USB, release **BOOT**).
  - On Linux, make sure your user is in the `dialout` group (`sudo usermod -aG dialout $USER`, then re-login).
- **Upload error: `Failed to connect to ESP32-C3`**:
  - Use the bootloader button sequence above (hold **BOOT**, tap **RST**, release **BOOT**) right before uploading.
- **Screen stays blank after flashing**:
  - Tap **RST** on the ESP32-C3.
  - Double-check wiring against [Step 1](#step-1-wire-the-display), especially `VCC` → `3.3V`, `GND`, `SCL` → `GPIO 4`, `SDA` → `GPIO 6`, and `BLK` → `GPIO 0` (or `3.3V`).
- **Won't connect to home Wi-Fi**:
  - Verify your network is **2.4 GHz** (5 GHz-only SSIDs will not work).
  - If connection fails 3 times, the device automatically falls back to **`ESP32-FlightClock`** setup mode so you can re-enter credentials.
- **`http://flightclock.local` doesn't load**:
  - Some networks and Android devices don't resolve `.local` (mDNS) addresses. Find the ESP32's IP address in your router's connected devices list and open `http://<IP_ADDRESS>` instead.
