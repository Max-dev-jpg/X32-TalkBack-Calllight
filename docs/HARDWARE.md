# Hardware Guide

## Bill of Materials

| Component | Specification | Notes |
|-----------|---------------|-------|
| ESP32 DevKit V1 | 38-pin, 4 MB flash | Main controller |
| WS2812B LED strip | 30–144 LEDs/m, 5 V | Call light output |
| 5 V relay module | Opto-isolated, active-high | Lamp control |
| 2N2222 or IRLZ44N transistor | NPN BJT / N-channel MOSFET | Direct lamp switch |
| 5 V power supply | ≥2 A for LEDs | Shared with ESP32 |
| 100 Ω resistor | LED data line protection | Recommended |
| 1000 µF capacitor | 5 V rail, electrolytic | Smoothing for LED strip |
| USB-A to Micro-USB cable | Data capable | Programming / power |

---

## GPIO Pinout – ESP32 DevKit V1

```
                    ┌─────────────┐
              3V3 ──┤ 3V3     GND ├── GND
              EN  ──┤ EN      D23 ├──
              D36 ──┤ VP      D22 ├──
              D39 ──┤ VN      TX0 ├──
              D34 ──┤ D34     RX0 ├──
              D35 ──┤ D35     D21 ├──
              D32 ──┤ D32     D19 ├──
              D33 ──┤ D33     D18 ├──
              D25 ──┤ D25      D5 ├──
              D26 ──┤ D26     TX2 ├──
              D27 ──┤ D27     RX2 ├──
              D14 ──┤ D14      D4 ├── WS2812B Data (default)
              D12 ──┤ D12      D2 ├── GPIO Output / Relay (default)
              GND ──┤ GND      D15├──
              D13 ──┤ D13     D13 ├──
              D9  ──┤ SD2     SD3 ├──
              D10 ──┤ SD3     CMD ├──
              D11 ──┤ CMD     SD0 ├──
              5V  ──┤ 5V      CLK ├──
                    └─────────────┘
```

**Default pin assignments (configurable in web UI):**

| Function | Default Pin | Notes |
|----------|-------------|-------|
| GPIO / Relay output | **GPIO 2** | Built-in LED on most DevKits |
| WS2812B data | **GPIO 4** | Use 100 Ω series resistor |

---

## Wiring Diagrams

### Option A: Relay Output (lamp/beacon control)

```
ESP32                Relay Module              Mains Lamp
GPIO 2 ─────────── IN                    ┌── COM ─── Live
GND   ─────────── GND                   │   NO  ─── Lamp ─── Live (return)
5V    ─────────── VCC    Relay ──────────┘   NC  (unused)
                                Neutral ────────────────── Lamp ─── Neutral
```

> ⚠️ **Safety warning:** Mains voltage work must comply with local electrical codes.
> Use a fully enclosed, rated relay module. Do NOT expose live conductors.

### Option B: Transistor Switch (12 V / 24 V DC warning lamp)

```
ESP32                        Warning Lamp (12 V DC)
GPIO 2 ─── 1 kΩ ──┬── Base
                   │
GND    ──────────── Emitter ─── GND ─── Lamp (–)
                   Collector ────────── Lamp (+)
12 V   ──────────────────────────────── PSU (+)

(MOSFET variant: Gate=GPIO via 100 Ω, Source=GND, Drain=Lamp–)
```

### Option C: WS2812B LED Strip

```
5 V PSU (+) ─── 1000 µF capacitor ─── WS2812B (+)
5 V PSU (–) ─────────────────────── WS2812B (–)
ESP32 GPIO 4 ─── 100 Ω ────────── WS2812B DIN
ESP32 GND ──────────────────────── WS2812B GND (common ground!)
```

> ⚠️ Always share ground between ESP32 and the LED strip PSU.
> Power long strips (>30 LEDs) from the strip's end, not only the beginning.

---

## Power Requirements

| Configuration | Minimum PSU |
|---------------|-------------|
| ESP32 only (USB) | 5 V 500 mA |
| ESP32 + 30 LEDs (white, full bright) | 5 V 2 A |
| ESP32 + 144 LEDs (white, full bright) | 5 V 9 A |
| Relay module | included in ESP32 budget |

For permanent theater installation, use a quality DIN-rail 5 V PSU rated at
**1.5× the calculated LED current** to stay within thermal limits.

---

## Enclosure Recommendations

- IP54 or better for wet/dusty stage environments
- DIN-rail mountable enclosures (e.g. Spelsberg TK PS series)
- Separate compartments for mains and low-voltage wiring
- Cable strain relief on all entries
