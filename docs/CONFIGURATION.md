# Configuration Reference

All settings are stored in ESP32 NVS (Non-Volatile Storage) and survive
reboots and power loss. Settings are managed through the web interface.

---

## Network Settings

| Parameter | Default | Description |
|-----------|---------|-------------|
| `apPassword` | `calllight` | Access Point password (8+ chars) |
| `wifiSSID` | _(empty)_ | Station WiFi network name |
| `wifiPassword` | _(empty)_ | Station WiFi password |
| `useDHCP` | `true` | Use DHCP for station IP |
| `staticIP` | `192.168.1.100` | Static IP (if DHCP disabled) |
| `staticGateway` | `192.168.1.1` | Default gateway |
| `staticSubnet` | `255.255.255.0` | Subnet mask |

**AP SSID** (`TalkBack-CallLight`) is fixed at compile time in `config.h`.

---

## Mixer Settings

| Parameter | Default | Description |
|-----------|---------|-------------|
| `mixerIP` | `192.168.0.100` | IP address of the X32/M32 |
| `oscTxPort` | `10023` | UDP port on the mixer (receives OSC) |
| `oscRxPort` | `10024` | UDP port on the ESP32 (receives responses) |
| `mixerType` | `0` (X32) | 0 = Behringer X32, 1 = Midas M32 |
| `channelType` | `3` (DCA) | 0=Input, 1=Bus, 2=Matrix, 3=DCA |
| `channelNumber` | `1` | Channel number (1-based) |
| `signalSource` | `0` (Fader) | 0=Fader, 1=Meter, 2=Mute state |
| `customOSCPath` | _(empty)_ | Override auto-generated path |

---

## Trigger Settings

| Parameter | Default | Description |
|-----------|---------|-------------|
| `threshold` | `0.50` | Signal level to activate (0.0–1.0) |
| `hysteresis` | `0.05` | Dead-band below threshold for deactivation |
| `smoothing` | `0.15` | EMA alpha: 0.01 = heavy, 1.0 = none |
| `holdTimeMs` | `500` | Minimum ON duration after trigger (ms) |
| `releaseDelayMs` | `1000` | Delay before output turns off (ms) |
| `debounceMs` | `50` | Minimum signal stable time before state change |

### Signal state machine

```
Signal rises above threshold
  → debounce (debounceMs)
    → TRIGGERED, start holdTimer
      Signal drops below (threshold - hysteresis)
        → hold period (holdTimeMs)
          → release delay (releaseDelayMs)
            → RELEASED
```

---

## Output Settings

| Parameter | Default | Description |
|-----------|---------|-------------|
| `outputType` | `2` (Both) | 0=GPIO only, 1=WS2812 only, 2=Both |
| `outputPin` | `2` | GPIO pin for relay/lamp |
| `outputInvert` | `false` | Invert polarity (active-low relay) |
| `flashMode` | `1` (Blink) | 0=Solid, 1=Blink, 2=Pulse, 3=Strobe |
| `flashSpeedMs` | `500` | Flash period in milliseconds |

---

## LED Settings

| Parameter | Default | Description |
|-----------|---------|-------------|
| `ledPin` | `4` | WS2812B data pin |
| `ledCount` | `30` | Number of LEDs in strip |
| `ledBrightness` | `128` | Global brightness (0–255) |
| `ledR` | `255` | Red component |
| `ledG` | `120` | Green component |
| `ledB` | `0` | Blue component (default: amber) |

---

## Factory Reset

Via web UI: **Tools → Factory Reset**

Via serial: Not implemented (resets only if NVS is fully erased)

Via code: Call `ConfigManager::instance().resetToDefaults()`
