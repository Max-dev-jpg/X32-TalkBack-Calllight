# OSC Reference – Behringer X32 / Midas M32

## Protocol Overview

The X32/M32 uses standard **OSC (Open Sound Control)** over **UDP**.

| Parameter | Value |
|-----------|-------|
| Protocol | OSC over UDP |
| Default TX Port (to mixer) | **10023** |
| Default RX Port (from mixer) | **10024** |
| Value range (faders) | 0.0 – 1.0 (float32) |

---

## Common OSC Paths

### Input Channels (01–32)
```
/ch/01/mix/fader    → fader level (float 0.0–1.0)
/ch/01/mix/on       → mute state (int 0=muted, 1=active)
/ch/01/mix/pan      → pan (-1.0 to +1.0)
```

### Bus Outputs (01–16)
```
/bus/01/mix/fader
/bus/01/mix/on
```

### Matrix Outputs (01–06)
```
/mtx/01/mix/fader
/mtx/01/mix/on
```

### DCA Groups (1–8)
```
/dca/1/fader
/dca/1/on
```

### Main / LR
```
/main/st/mix/fader
/main/st/mix/on
```

---

## Querying a Value

Send an empty OSC message (no arguments) to the path.
The X32 responds with the same path and the current value as the argument.

**Example: Query DCA 1 fader**
```
Send:    /dca/1/fader  (no args)
Receive: /dca/1/fader  ,f  [float bytes]
```

---

## Subscription – /xremote

To receive push notifications when parameters change:

```
Send: /xremote  (no args)  every <10 seconds
```

After subscribing, the X32 pushes parameter changes as they happen.
The TalkBack CallLight renews `/xremote` every **8 seconds** automatically.

---

## Meter Data – /meters

```
Send:    /meters  ,s  "/meters/0"
Receive: /meters/0  ,b  [blob: channel meters as int16[]]
```

Each channel value is an `int16` (0–32767), representing 0–100% level.
The blob ordering follows the X32 channel assignment.

Meter indexes:
| Index | Contents |
|-------|----------|
| /meters/0 | Input channels 1–32 pre-fader |
| /meters/1 | Input channels 1–32 post-fader |
| /meters/2 | Bus outputs |
| /meters/3 | DCA master levels |

---

## fader → dB Conversion

The X32 fader value is logarithmically mapped:

| Fader value | Approximate dB |
|-------------|----------------|
| 1.000 | 0 dBFS (unity) |
| 0.750 | −10 dB |
| 0.500 | −30 dB |
| 0.250 | −60 dB |
| 0.000 | −∞ (off) |

---

## Testing with Python

See [examples/osc_test.py](../examples/osc_test.py) for a ready-to-run script.

```bash
python examples/osc_test.py --ip 192.168.0.100 --path /dca/1/fader
```
