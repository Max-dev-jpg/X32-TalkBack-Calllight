#pragma once
// =============================================================================
// config.h  –  Global constants and compile-time defaults
// =============================================================================

// ── Firmware identity ─────────────────────────────────────────────────────────
#define FIRMWARE_VERSION    "1.1.0"
#define DEVICE_NAME         "TalkBack-CallLight"

// ── Access Point defaults (user can change password via web UI) ───────────────
#define DEFAULT_AP_SSID        "TalkBack-CallLight"
#define DEFAULT_AP_PASSWORD    "calllight"
#define AP_CHANNEL             1
#define AP_MAX_CONNECTIONS     4

// ── Network defaults ──────────────────────────────────────────────────────────
#define DEFAULT_MIXER_IP       "192.168.0.100"
#define DEFAULT_OSC_TX_PORT    10023   // X32 receives on this port
#define DEFAULT_OSC_RX_PORT    10024   // we listen on this port
#define OSC_POLL_INTERVAL_MS   200     // how often we poll the mixer
#define XREMOTE_INTERVAL_MS    8000    // /xremote renewal period (< 10 s)
#define MIXER_TIMEOUT_MS       3000    // declare lost after this silence

// ── GPIO defaults ─────────────────────────────────────────────────────────────
#define DEFAULT_OUTPUT_PIN     2       // built-in LED on most DevKits
#define DEFAULT_LED_DATA_PIN   4
#define DEFAULT_LED_COUNT      30
#define DEFAULT_LED_BRIGHTNESS 128     // 0-255

// ── Trigger defaults ──────────────────────────────────────────────────────────
#define DEFAULT_THRESHOLD        0.50f   // 0.0-1.0
#define DEFAULT_HOLD_TIME_MS     500
#define DEFAULT_RELEASE_DELAY_MS 1000
#define DEFAULT_HYSTERESIS       0.05f
#define DEFAULT_SMOOTHING        0.15f   // EMA alpha
#define DEFAULT_DEBOUNCE_MS      50

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
#define CH_MONO    7   // Main Mono/Center (no number)

#define SIG_FADER  0   // continuous 0.0-1.0  (polled via /xremote)
#define SIG_METER  1   // meter level 0.0-1.0  (subscribed via /batchsubscribe)
#define SIG_MUTE   2   // 0=muted, 1=active  (treated as 0.0/1.0)

// ── NVS storage ───────────────────────────────────────────────────────────────
#define NVS_NAMESPACE  "talkback"

// ── Web server ────────────────────────────────────────────────────────────────
#define WEB_SERVER_PORT       80
#define WEBSOCKET_PATH        "/ws"
#define WS_BROADCAST_INTERVAL 1000    // ms between status broadcasts

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
#define WIFI_RECONNECT_INTERVAL_MS   10000
#define MIXER_RECONNECT_INTERVAL_MS   5000

// ── Talkback Engine ───────────────────────────────────────────────────────────
// OSC paths for talkback button state on X32/M32
#define TB_PATH_A            "/-stat/talk/A"
#define TB_PATH_B            "/-stat/talk/B"
#define TB_CLEARSOLO_PATH    "/-action/clearsolo"
#define TB_SOLOSW_BASE       "/-stat/solosw/"   // append 2-digit 1-based ID
#define TB_POLL_INTERVAL_MS  100   // how often to poll talkback state
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

// ── Multi-trigger ─────────────────────────────────────────────────────────────
#define MAX_TRIGGERS  4

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
#define XOSC_PATH_TRIGGER      "/calllight/trigger"
#define XOSC_PATH_TRIGGER_ON   "/calllight/trigger/on"
#define XOSC_PATH_TRIGGER_OFF  "/calllight/trigger/off"
#define XOSC_PATH_TB_A         "/calllight/talkback/a"
#define XOSC_PATH_TB_B         "/calllight/talkback/b"
