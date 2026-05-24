# Bill of Materials

## Core Components

| Qty | Part | Description | Example Part |
|-----|------|-------------|--------------|
| 1 | ESP32 DevKit V1 | 38-pin ESP32 development board | AZ-Delivery ESP32 DevKitC |
| 1 | 5 V PSU | DIN-rail or desktop, ≥2 A | MW RS-25-5 |
| 1 | Micro-USB cable | Data + power, ≥0.5 m | — |

## Output – Option A: WS2812B LED Strip

| Qty | Part | Description |
|-----|------|-------------|
| 1 | WS2812B strip | 5 V, 30–60 LEDs/m, IP65 for stage use |
| 1 | 100 Ω resistor | 1/4 W, data line series protection |
| 1 | 1000 µF / 10 V capacitor | Electrolytic, PSU smoothing |

## Output – Option B: Relay (mains lamp)

| Qty | Part | Description |
|-----|------|-------------|
| 1 | Relay module | 5 V coil, opto-isolated, 10 A / 250 VAC |
| 1 | Warning lamp | 230 VAC E27 or signal lamp |
| — | Mains cable | Appropriate rated cable |

## Output – Option C: Transistor (12/24 V DC lamp)

| Qty | Part | Description |
|-----|------|-------------|
| 1 | IRLZ44N MOSFET | Logic-level gate, N-channel, TO-220 |
| 1 | 100 Ω resistor | Gate series resistor |
| 1 | 1N4007 diode | Flyback diode for inductive load |
| 1 | 12/24 V warning lamp | Or beacon/strobe, rated for voltage |

## Enclosure (Permanent Theater Install)

| Qty | Part | Description |
|-----|------|-------------|
| 1 | Spelsberg TK PS 1611-6-m | Polystyrene enclosure 160×110×61 mm |
| 1 | DIN rail 35 mm | Fits relay and PSU modules |
| 4 | M3×10 standoffs | PCB mounting |
| — | Cable glands PG9 | For cable entries |

## Estimated Cost (without lamp/cable)

| Config | Approx. cost |
|--------|-------------|
| ESP32 + WS2812B 30 LED | €25–35 |
| ESP32 + relay module | €20–30 |
| Full enclosure + PSU + relay | €60–90 |
