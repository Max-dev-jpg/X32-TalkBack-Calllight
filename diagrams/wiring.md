# Wiring Diagrams

## System Block Diagram

```
┌──────────────────────────────────────────────────────────────┐
│                    Theater Network                           │
│                                                              │
│  ┌──────────────┐   OSC/UDP     ┌─────────────────────────┐ │
│  │ Behringer X32│──────────────▶│   TalkBack CallLight    │ │
│  │ or Midas M32 │               │      ESP32 DevKit V1    │ │
│  └──────────────┘               │                         │ │
│                                 │  AP: TalkBack-CallLight  │ │
│  ┌──────────────┐   WiFi / AP  │  http://192.168.4.1      │ │
│  │  Tablet /    │◀────────────▶│  http://talkback-.local  │ │
│  │  Smartphone  │   Web UI     │                         │ │
│  └──────────────┘               └──────────┬────────────┘ │
│                                            │               │
└────────────────────────────────────────────┼───────────────┘
                                             │
                             ┌───────────────┴──────────────┐
                             │        Output Stage          │
                             │                              │
                             │  GPIO 2 ──▶ Relay / MOSFET  │
                             │  GPIO 4 ──▶ WS2812B Strip   │
                             └──────────────────────────────┘
```

---

## Minimal Wiring (WS2812B + Relay)

```
    5V PSU
    +5V ──────────────────────┬─────────────────────────────┐
                              │                             │
                              │              ESP32 DevKit   │
    GND ──────────────────────┤         ┌───────────────┐   │
                              │         │               │   │
                              ├────────▶│ 5V        GND ├───┘
                              │         │               │
                              │         │ GPIO 4        ├──── 100Ω ──▶ WS2812B DIN
                              │         │               │
                              │         │ GPIO 2        ├──────────▶ Relay IN
                              │         │               │
                              │         │ GND           ├──── WS2812B GND
                              │         │               │
                              │         └───────────────┘
                              │
   WS2812B Strip              │
    +5V ◀─────────────────────┘
    GND ◀──────── (see above)
    DIN ◀──────── GPIO 4 via 100 Ω
```

---

## Relay Module Wiring

```
ESP32 GPIO 2  ──────────────▶  Relay IN
ESP32 GND     ──────────────▶  Relay GND
5V            ──────────────▶  Relay VCC

Relay COM ──────────────────── Mains Live (IN)
Relay NO  ──────────────────── Lamp Live (OUT)
Mains Neutral ──────────────── Lamp Neutral
```

---

## MOSFET Wiring (12 V DC lamp)

```
ESP32 GPIO 2  ─── 100 Ω ─── IRLZ44N Gate
ESP32 GND     ─────────────  IRLZ44N Source ─── GND
                              IRLZ44N Drain  ─── Lamp (–)
12 V PSU (+)  ─────────────  Lamp (+)
12 V PSU (–)  ─────────────  GND (common with ESP32)

Flyback diode: 1N4007 across lamp terminals (cathode to +)
```
