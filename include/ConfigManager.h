#pragma once
// =============================================================================
// ConfigManager.h  –  Central configuration struct + singleton manager
// =============================================================================

#include <Arduino.h>
#include "config.h"

// All device settings in one flat struct — serialised to/from NVS.
struct DeviceConfig {
    // ── Network ──────────────────────────────────────────────────────────────
    char apPassword[64];
    char wifiSSID[64];
    char wifiPassword[64];
    bool useDHCP;
    char staticIP[16];
    char staticGateway[16];
    char staticSubnet[16];

    // ── Mixer ────────────────────────────────────────────────────────────────
    char     mixerIP[16];
    uint16_t oscTxPort;
    uint16_t oscRxPort;
    uint8_t  mixerType;      // MIXER_X32 / MIXER_M32
    uint8_t  channelType;    // CH_INPUT / CH_BUS / CH_MATRIX / CH_DCA
    uint8_t  channelNumber;  // 1-32
    uint8_t  signalSource;   // SIG_FADER / SIG_METER / SIG_MUTE
    char     customOSCPath[64]; // overrides auto-generated path when non-empty

    // ── Trigger ──────────────────────────────────────────────────────────────
    float    threshold;
    uint32_t holdTimeMs;
    uint32_t releaseDelayMs;
    float    hysteresis;
    float    smoothing;       // EMA alpha  (0.0-1.0, higher = more responsive)
    uint32_t debounceMs;

    // ── Output ───────────────────────────────────────────────────────────────
    uint8_t  outputType;     // OUTPUT_GPIO / OUTPUT_WS2812 / OUTPUT_BOTH
    uint8_t  outputPin;
    bool     outputInvert;   // invert GPIO polarity (active-low relay)
    uint8_t  flashMode;      // FLASH_SOLID / BLINK / PULSE / STROBE
    uint16_t flashSpeedMs;

    // ── LED strip ────────────────────────────────────────────────────────────
    uint8_t  ledPin;
    uint16_t ledCount;
    uint8_t  ledBrightness;
    uint8_t  ledR;
    uint8_t  ledG;
    uint8_t  ledB;

    // ── Talkback Engine ───────────────────────────────────────────────────────
    // Each list stores a JSON array of action objects, e.g.:
    //   [{"t":"clearSolo"},{"t":"solo","ct":0,"cn":1},{"t":"out","s":true}]
    // Action types: clearSolo | solo | unsolo | mute | unmute | osc | out
    //   solo/unsolo/mute/unmute: ct=chType, cn=chNum (1-based)
    //   osc:  p=oscPath (string), v=value (int)
    //   out:  s=state (bool) — forces call-light output on/off
    bool     tbEnabled;       // enable the talkback engine
    uint8_t  tbMonitor;       // TB_MONITOR_A / _B / _BOTH
    char     tbAOnJson [TB_ACTION_JSON_LEN];  // actions when Talk A activates
    char     tbAOffJson[TB_ACTION_JSON_LEN];  // actions when Talk A releases
    char     tbBOnJson [TB_ACTION_JSON_LEN];  // actions when Talk B activates
    char     tbBOffJson[TB_ACTION_JSON_LEN];  // actions when Talk B releases
};

// =============================================================================
// ConfigManager  –  loads defaults, persists via StorageManager
// =============================================================================
class ConfigManager {
public:
    static ConfigManager& instance() {
        static ConfigManager inst;
        return inst;
    }

    // Initialise: fill defaults, then overlay saved NVS values
    void begin();

    // Save all settings to NVS
    bool save();

    // Reset to compile-time defaults and save
    void resetToDefaults();

    // Build the OSC path for the currently configured channel + signal
    String buildOSCPath() const;

    // Direct access to the config struct
    DeviceConfig& cfg() { return _cfg; }
    const DeviceConfig& cfg() const { return _cfg; }

private:
    ConfigManager() {}
    void applyDefaults();
    DeviceConfig _cfg;
};

// Convenience global accessor
#define Config ConfigManager::instance().cfg()
