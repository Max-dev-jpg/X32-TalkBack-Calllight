# TalkBack CallLight

> ESP32-based call light controller for **Behringer X32** and **Midas M32** mixing consoles.
> Designed for permanent installation in theater and stage environments.

---

## Overview

TalkBack CallLight monitors a configurable channel (fader, meter, or mute state) on an X32 or M32 mixer via OSC over UDP. When the signal exceeds a configured threshold, it triggers a visual call light — a WS2812B LED strip, a relay-driven lamp, or both — with configurable flash modes.

The ESP32 hosts a responsive dark-mode web interface for all settings. All configuration is stored persistently in NVS and survives reboots and power loss.

---

## Features

- **OSC monitoring** — polls any fader, meter level, or mute state via configurable OSC path
- **AP + STA dual mode** — access point always on; optionally joins your venue WiFi
- **Responsive web UI** — dark mode, mobile-friendly, live WebSocket status
- **WS2812B LED strip** — solid, blink, pulse (sine fade), strobe flash modes
- **GPIO output** — drives relay, MOSFET, or any logic-level device
- **Persistent settings** — all config stored in ESP32 NVS
- **OTA updates** — update firmware over WiFi without USB
- **mDNS** — reachable at `http://talkback-calllight.local`
- **Non-blocking** — `millis()`-based timing throughout; no `delay()` in main loop

---

## Hardware Requirements

| Component | Notes |
|-----------|-------|
| ESP32 DevKit V1 | Any 38-pin ESP32 with 4 MB flash |
| WS2812B LED strip | 5 V, any length (configure count in web UI) |
| 5 V relay module | Opto-isolated recommended |
| 5 V power supply | ≥2 A for strip; USB for dev |

See [docs/HARDWARE.md](docs/HARDWARE.md) for full BOM, GPIO pinout, and wiring diagrams.

---

## Quick Start

### 1. Clone the repository

```bash
git clone https://github.com/YOUR_USERNAME/X32-TalkBack-Calllight.git
cd X32-TalkBack-Calllight
```

### 2. Install PlatformIO

**macOS / Linux:**
```bash
pip3 install platformio
# or via Homebrew:
brew install platformio
```

**Windows:**
```powershell
pip install platformio
```

Or install the **PlatformIO IDE extension** for VS Code (all platforms).

### 3. Build & flash firmware

```bash
# Build
pio run

# Flash firmware
pio run --target upload

# Flash web UI files (LittleFS)
pio run --target uploadfs

# Open serial monitor
pio device monitor
```

> On first flash, upload **both** `upload` (firmware) and `uploadfs` (web files).

### 4. Connect to the device

1. Connect to WiFi AP: **`TalkBack-CallLight`** / password **`calllight`**
2. Open browser: **`http://192.168.4.1`**
3. Or after joining your venue WiFi: **`http://talkback-calllight.local`**

---

## Setup Guide

### Network
1. Open **Network** tab in the web UI
2. Enter your venue WiFi SSID and password
3. Click **Save & Apply**
4. Device reboots and joins your network; AP stays active

### Mixer
1. Open **Mixer** tab
2. Set your X32/M32 IP address (e.g. `192.168.0.100`)
3. Select channel type and number (e.g. DCA 1)
4. Select signal source (Fader Level recommended)
5. Click **Save & Apply**

### Trigger
1. Open **Trigger** tab
2. Watch the live level bar on the **Status** tab
3. Set threshold just above your ambient noise floor (e.g. `0.50`)
4. Adjust hold time and release delay to taste
5. Click **Save & Apply**

### Output
1. Open **Output** tab
2. Set output type (GPIO, WS2812, or both)
3. Select flash mode and speed
4. Pick LED color (amber is standard for call lights)
5. Click **Save & Apply**
6. Use **Tools → Test Output** to verify wiring

---

## Web Interface

| Tab | Contents |
|-----|----------|
| **Status** | Live mixer level, trigger state, WiFi, uptime, heap |
| **Network** | WiFi credentials, DHCP/static IP, AP info |
| **Mixer** | Mixer IP, OSC port, channel, signal source, custom path |
| **Trigger** | Threshold, hysteresis, smoothing, hold time, release delay |
| **Output** | GPIO pin, flash mode, LED strip settings, color |
| **Tools** | Test output, reconnect mixer, reboot, factory reset |

---

## OSC Paths

| Channel | Example Path |
|---------|-------------|
| Input 1 fader | `/ch/01/mix/fader` |
| Bus 2 fader | `/bus/02/mix/fader` |
| DCA 1 fader | `/dca/1/fader` |
| DCA 1 mute | `/dca/1/on` |
| Custom | Set in Mixer → Custom OSC Path |

See [docs/OSC_REFERENCE.md](docs/OSC_REFERENCE.md) for full path list.

---

## Wiring

### WS2812B LED Strip
```
ESP32 GPIO 4 ─── 100 Ω ───▶ Strip DIN
ESP32 GND    ───────────────▶ Strip GND   (common ground!)
5V PSU (+)   ───────────────▶ Strip +5V
```

### Relay (mains lamp)
```
ESP32 GPIO 2 ───────────────▶ Relay IN
ESP32 GND    ───────────────▶ Relay GND
5V PSU       ───────────────▶ Relay VCC
Relay NO     ───────────────▶ Lamp Live (out)
Relay COM    ───────────────▶ Mains Live (in)
```

See [diagrams/wiring.md](diagrams/wiring.md) for ASCII wiring diagrams.

---

## Project Structure

```
├── src/                    C++ source files
│   ├── main.cpp            Entry point (setup + loop)
│   ├── ConfigManager.cpp   Configuration struct + defaults
│   ├── StorageManager.cpp  NVS Preferences wrappers
│   ├── NetworkManager.cpp  WiFi AP+STA, mDNS, OTA
│   ├── MixerConnection.cpp OSC/UDP link to X32/M32
│   ├── OSCHandler.cpp      Lightweight OSC encoder/decoder
│   ├── TriggerLogic.cpp    Smoothing, threshold, hold, release
│   ├── OutputController.cpp GPIO output + flash modes
│   ├── LEDController.cpp   WS2812B NeoPixel control
│   └── WebServerManager.cpp Async web server + REST + WebSocket
├── include/                Header files
├── data/                   Web UI (served from LittleFS)
│   ├── index.html
│   ├── style.css
│   └── app.js
├── docs/                   Documentation
├── examples/               Python OSC test scripts
├── hardware/               BOM and hardware notes
├── diagrams/               Wiring diagrams
├── platformio.ini          Build configuration
└── README.md
```

---

## Build Environments

| Environment | Use |
|-------------|-----|
| `esp32dev` | Production build |
| `esp32dev_debug` | Verbose serial logging |
| `esp32dev_ota` | OTA upload (requires device on network) |

```bash
pio run -e esp32dev_debug        # debug build
pio run -e esp32dev_ota --target upload  # OTA update
```

---

## Troubleshooting

### Device not found at 192.168.4.1
- Connect to the `TalkBack-CallLight` AP first
- Check AP password is `calllight` (default)
- Try `http://talkback-calllight.local` if on the same network

### Mixer not connecting
- Verify mixer IP in Mixer settings
- Confirm X32 is on the same network as the ESP32
- Use **Tools → Reconnect Mixer**
- Run `examples/osc_test.py` to verify the X32 responds to OSC queries

### LED strip not lighting
- Verify data pin matches **Output → LED Data Pin**
- Check common GND between ESP32 and strip PSU
- Add 100 Ω series resistor on data line
- Verify **Output Type** is set to WS2812 or Both

### Web UI missing after flash
- Run `pio run --target uploadfs` to upload the LittleFS filesystem
- This is separate from the firmware upload

### OTA update fails
- Confirm ESP32 and computer are on the same network
- Check `upload_port` in `platformio.ini` matches mDNS hostname or IP

---

## Roadmap

- [ ] Multi-channel monitoring (trigger any of N channels)
- [ ] Ethernet / PoE support (W5500 SPI module)
- [ ] Web authentication (username + password)
- [ ] Firmware update via web UI (drag & drop .bin)
- [ ] MQTT output for home automation integration
- [ ] Logging page with circular buffer
- [ ] Multiple independent outputs with separate thresholds

---

## License

MIT — see [LICENSE](LICENSE)

---

## Screenshots

> _Add screenshots of the web UI to `screenshots/` and reference them here._

![Status tab](screenshots/status.png)
![Mixer settings](screenshots/mixer.png)
