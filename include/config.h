#pragma once
// =============================================================================
// config.h  –  Global constants and compile-time defaults
// =============================================================================

// ── Serial prints ─────────────────────────────────────────────────────────
#define DEBUG 1

#if DEBUG == 1
  #define DBG_BEGIN(...) Serial.begin(__VA_ARGS__)
  #define DBG_PRINT(...) Serial.print(__VA_ARGS__)
  #define DBG_PRINTLN(...) Serial.println(__VA_ARGS__)
  #define DBG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
  #define DBG_BEGIN(...)
  #define DBG_PRINT(...)
  #define DBG_PRINTLN(...)
  #define DBG_PRINTF(...)
#endif

// ── Firmware identity ─────────────────────────────────────────────────────────
#define FIRMWARE_VERSION    "2.1.0"
#define DEVICE_NAME         "TalkBack-CallLight"

// ── Boot ──────────────────────────────────────────────────────────────────────
// On a marginal supply the WiFi power-up current spike can dip the rail below the
// brownout trip point and reboot-loop the ESP32 before it starts. Disabling the
// detector lets it boot. This removes low-voltage protection — the real fix is a
// stiffer 5 V supply / a bulk capacitor near the board. Set to 0 to keep it on.
#define DISABLE_BROWNOUT_DETECTOR 0

// ── Access Point defaults (user can change password via web UI) ───────────────
#define DEFAULT_AP_SSID        "TalkBack-CallLight"
#define DEFAULT_AP_PASSWORD    "" //no password
#define AP_CHANNEL             1
#define AP_MAX_CONNECTIONS     4

// ── Network defaults ──────────────────────────────────────────────────────────
#define DEFAULT_MIXER_IP       "192.168.1.3"
#define DEFAULT_OSC_TX_PORT    10023   // X32 receives on this port
#define DEFAULT_OSC_RX_PORT    10024   // we listen on this port
#define OSC_POLL_INTERVAL_MS   200     // unused: fader/mute queries run at XREMOTE_INTERVAL_MS cadence
#define XREMOTE_INTERVAL_MS    8000    // /xremote renewal period (< 10 s)
#define MIXER_TIMEOUT_MS       10000   // must be > XREMOTE_INTERVAL_MS to avoid false disconnects
#define MIXER_KEEPALIVE_INTERVAL_MS  4000  // /info keepalive to sustain connection when mixer is idle

// ── GPIO defaults ─────────────────────────────────────────────────────────────
#define DEFAULT_OUTPUT_PIN     2       // built-in LED on most DevKits
#define DEFAULT_LED_DATA_PIN   4
#define DEFAULT_LED_COUNT      4
#define DEFAULT_LED_BRIGHTNESS 128     // 0-255

// ── Trigger defaults ──────────────────────────────────────────────────────────
// Threshold + hysteresis are in dB (meter/fader levels are converted to dB in
// the firmware; mute ignores the threshold). Smoothing is a dimensionless EMA α.
#define DEFAULT_THRESHOLD        -20.0f  // dB
#define DEFAULT_HOLD_TIME_MS     500
#define DEFAULT_RELEASE_DELAY_MS 1000
#define DEFAULT_HYSTERESIS       3.0f    // dB
#define DEFAULT_SMOOTHING        0.15f   // EMA alpha (0.01-1.0)
#define DEFAULT_DEBOUNCE_MS      50

// The EMA smoothing is advanced one step per this many milliseconds — NOT once
// per main-loop iteration (the loop runs thousands of times per second, which
// would make the filter converge almost instantly and the smoothing invisible).
// With this fixed tick, alpha is a real time constant: τ ≈ interval / alpha
// (e.g. alpha 0.15 → ~130 ms, alpha 0.01 → ~2 s). Roughly matches the ~50 ms
// meter frame rate while giving sub-frame resolution.
#define TRIG_SMOOTHING_INTERVAL_MS  20

// Values within this many dB of a source's "quiet" end (no gain reduction / -60 /
// -90 / unmuted) are snapped to that end before the threshold compare. This keeps
// meter noise + EMA residual near the floor from holding a trigger active — a
// compressor/gate meter reads a little reduction even when idle. It does NOT shift
// a meaningful threshold, only the quiet extreme. Raise it if an idle compressor
// still triggers at threshold 0 (check the OSC monitor for its resting dB value).
#define TRIG_FLOOR_DEADBAND_DB   0.05f
// Same idea for a custom-OSC-path trigger, whose value lives in a linear 0..1
// domain instead of dB — the dB deadband above is meaningless in that range.
#define TRIG_FLOOR_DEADBAND_LINEAR  0.005f

// ── Flash modes ───────────────────────────────────────────────────────────────
#define FLASH_SOLID    0
#define FLASH_BLINK    1
#define FLASH_PULSE    2
#define FLASH_STROBE   3
#define DEFAULT_FLASH_MODE     FLASH_BLINK
#define DEFAULT_FLASH_SPEED_MS 500

// ── Output types ──────────────────────────────────────────────────────────────
#define OUTPUT_GPIO    0   // simple GPIO high/low  (also drives relay/transistor)
#define OUTPUT_WS2812  1   // NeoPixel LED strip
#define OUTPUT_BOTH    2   // GPIO + LED strip simultaneously

// ── Channel / signal source enums ────────────────────────────────────────────
#define CH_INPUT   0   // Input channels  1-32
#define CH_BUS     1   // Mix buses        1-16
#define CH_MATRIX  2   // Matrix           1-6
#define CH_DCA     3   // DCA groups       1-8  (fader/mute only – not meters)
#define CH_AUXIN   4   // Aux inputs       1-8
#define CH_FXRTN   5   // FX returns       1-8
#define CH_MAIN    6   // Main L/R stereo  (no number)
#define CH_MONO    7   // Main M/C (no number)

#define SIG_FADER  0   // continuous 0.0-1.0  (polled via /xremote)
#define SIG_METER  1   // meter level 0.0-1.0  (subscribed via /batchsubscribe)
#define SIG_MUTE   2   // 0=muted, 1=active  (treated as 0.0/1.0)

// ── NVS storage ───────────────────────────────────────────────────────────────
#define NVS_NAMESPACE  "talkback"

// ── Web server ────────────────────────────────────────────────────────────────
#define WEB_SERVER_PORT       80
#define WEBSOCKET_PATH        "/ws"
#define WS_BROADCAST_INTERVAL 300    // ms between status broadcasts

// ── mDNS / OTA ───────────────────────────────────────────────────────────────
#define MDNS_HOSTNAME     "talkback-calllight"
#define OTA_PASSWORD      ""    // empty = no OTA password (set your own!)

// ── Serial ───────────────────────────────────────────────────────────────────
#define SERIAL_BAUD  115200

// ── Default LED color (amber/warning) ────────────────────────────────────────
#define DEFAULT_LED_R  255
#define DEFAULT_LED_G  120
#define DEFAULT_LED_B  0

// ── Reconnect intervals ───────────────────────────────────────────────────────
#define WIFI_RECONNECT_STA_INTERVAL_MS   10000
#define WIFI_SCAN_INTERVAL_AP_WITH_CLIENTS_MS   120000
#define WIFI_SCAN_INTERVAL_AP_NO_CLIENTS_MS   30000
#define MIXER_RECONNECT_INTERVAL_MS   5000

// ── Talkback Engine ───────────────────────────────────────────────────────────
// OSC paths for talkback button state on X32/M32
#define TB_PATH_A            "/-stat/talk/A"
#define TB_PATH_B            "/-stat/talk/B"
#define TB_CLEARSOLO_PATH    "/-action/clearsolo"
#define TB_SOLOSW_BASE       "/-stat/solosw/"   // append 2-digit 1-based ID
#define TB_RX_PORT           10025 // separate listen port for TalkbackEngine

// Talkback monitor selection
#define TB_MONITOR_A    0
#define TB_MONITOR_B    1
#define TB_MONITOR_BOTH 2

// Solo bus IDs (X32/M32 /-stat/solosw/{id}, 1-indexed)
// Input CH  :  1-32   (offset  0, 1-indexed)
// AuxIn     : 33-40   (offset 32)
// FX Return : 41-48   (offset 40)
// Bus       : 49-64   (offset 48)
// Matrix    : 65-70   (offset 64)
// Main LR   : 71
// Main Mono : 72
// DCA       : 73-80   (offset 72)
#define SOLO_OFFSET_INPUT   0
#define SOLO_OFFSET_AUXIN   32
#define SOLO_OFFSET_FXRTN   40
#define SOLO_OFFSET_BUS     48
#define SOLO_OFFSET_MATRIX  64
#define SOLO_OFFSET_DCA     72
#define SOLO_ID_MAIN_LR     71
#define SOLO_ID_MAIN_MONO   72

// Talkback / trigger action JSON buffer length (per list)
#define TB_ACTION_JSON_LEN  1024

// ── Meter subscription handling ──────────────────────────────────────────────
// /batchsubscribe time factor (tf, the last int in the request). The console
// sends each meter blob every ~tf × 50 ms: 1 = fastest (~20 Hz), 2 = ~10 Hz, …
// Lower = smoother/more responsive triggers but more UDP traffic + CPU; raise it
// to lighten the load at the cost of coarser metering.
#define METER_SUBSCRIBE_TF  1
// Meter subscriptions (/batchsubscribe) last ~10 s on the console and are kept
// alive with a no-arg /renew sent every METER_RENEW_INTERVAL_MS. Keep this well
// below 10000; a lower value tolerates more lost /renew packets (a renew that
// arrives after the 10 s timeout has no effect).
#define METER_RENEW_INTERVAL_MS  8000
// If a /renew is lost the subs lapse and meter data stops; a meter-data stall
// longer than this re-registers them (self-heal).
#define METER_STALL_MS  3000

// ── Multi-trigger ─────────────────────────────────────────────────────────────
#define MAX_TRIGGERS  4

// Multi-trigger simultaneous priority modes
#define PRIO_NEWEST  0   // most recently activated trigger's color wins
#define PRIO_FIXED   1   // static priority order from trigPriorityOrder[]

// ── Action Engine ─────────────────────────────────────────────────────────────
// Source IDs used as bit positions in ActionEngine's output bitmask (uint8_t)
#define ACT_SRC_TRIGGER_0  0   // Trigger 1
#define ACT_SRC_TRIGGER_1  1   // Trigger 2
#define ACT_SRC_TRIGGER_2  2   // Trigger 3
#define ACT_SRC_TRIGGER_3  3   // Trigger 4
#define ACT_SRC_TB_A       4   // Talkback A button
#define ACT_SRC_TB_B       5   // Talkback B button
#define ACT_SRC_OSC        6   // External OSC receiver
#define ACT_SEND_PORT      10026  // ActionEngine send-only UDP port

// ── External OSC Control (Companion / OSC controllers) ───────────────────────
#define DEFAULT_EXT_OSC_PORT   8000
// Legacy paths — Trigger 1 only, kept for backward compatibility
#define XOSC_PATH_TRIGGER      "/calllight/trigger"
#define XOSC_PATH_TRIGGER_ON   "/calllight/trigger/on"
#define XOSC_PATH_TRIGGER_OFF  "/calllight/trigger/off"
// Talkback paths
#define XOSC_PATH_TB_A         "/calllight/talkback/a"
#define XOSC_PATH_TB_B         "/calllight/talkback/b"
// Per-trigger base path — append /N, /N/on, /N/off, /N/pulse  (N = 1-4)
#define XOSC_PATH_TRIG_BASE    "/calllight/trigger/"
// Default pulse duration in ms (used when no argument is supplied)
#define XOSC_PULSE_DEFAULT_MS  300
