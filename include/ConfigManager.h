#pragma once
// =============================================================================
// ConfigManager.h  –  Central configuration struct + singleton manager
// =============================================================================

#include <Arduino.h>
#include "config.h"

// ── Per-trigger configuration ─────────────────────────────────────────────────
// Each trigger instance has its own channel source, signal processing params,
// invert flag, and on/off action lists.
struct TriggerConfig {
    bool     enabled;
    // ── Channel source ────────────────────────────────────────────────────────
    uint8_t  channelType;       // CH_INPUT / CH_BUS / CH_MATRIX / CH_DCA /
                                // CH_AUXIN / CH_FXRTN / CH_MAIN / CH_MONO
    uint8_t  channelNumber;     // 1-based; unused for CH_MAIN / CH_MONO
    uint8_t  signalSource;      // SIG_FADER / SIG_METER / SIG_MUTE
    char     customOSCPath[64]; // overrides auto-built path when non-empty
    // ── Signal processing ────────────────────────────────────────────────────
    float    threshold;
    float    hysteresis;
    float    smoothing;         // EMA alpha (0.01 – 1.0, higher = faster)
    uint32_t holdTimeMs;
    uint32_t releaseDelayMs;
    uint32_t debounceMs;
    bool     invert;            // if true: trigger fires when signal < threshold
    // ── Per-trigger LED color ─────────────────────────────────────────────────
    bool     useTriggerColor;   // if true: override global LED color when active
    uint8_t  trigLedR;
    uint8_t  trigLedG;
    uint8_t  trigLedB;
    // ── Actions ──────────────────────────────────────────────────────────────
    char     onJson [TB_ACTION_JSON_LEN]; // JSON array, executed on activate
    char     offJson[TB_ACTION_JSON_LEN]; // JSON array, executed on release
};

// ── All device settings in one flat struct ────────────────────────────────────
// Serialised to/from NVS by StorageManager.
struct DeviceConfig {
    // ── Network ──────────────────────────────────────────────────────────────
    char apPassword[64];
    char wifiSSID[64];
    char wifiPassword[64];
    bool useDHCP;
    char staticIP[16];
    char staticGateway[16];
    char staticSubnet[16];

    // ── Mixer connection ─────────────────────────────────────────────────────
    char     mixerIP[16];
    uint16_t oscTxPort;
    uint16_t oscRxPort;

    // ── Multi-trigger ────────────────────────────────────────────────────────
    TriggerConfig triggers[MAX_TRIGGERS];

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
    // Action types: clearSolo | solo | unsolo | mute | unmute | osc | out
    bool     tbEnabled;
    uint8_t  tbMonitor;       // TB_MONITOR_A / _B / _BOTH
    char     tbAOnJson [TB_ACTION_JSON_LEN];
    char     tbAOffJson[TB_ACTION_JSON_LEN];
    char     tbBOnJson [TB_ACTION_JSON_LEN];
    char     tbBOffJson[TB_ACTION_JSON_LEN];

    // ── External OSC Receiver ─────────────────────────────────────────────────
    bool     extOscEnabled;
    uint16_t extOscPort;
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

    void begin();
    bool save();
    void resetToDefaults();

    // Build the X32 OSC path for a given trigger's channel + signal config
    String  buildOSCPathForTrigger(const TriggerConfig& t) const;

    // Return the /meters/6 channel index for a trigger's channel type/number
    // Returns -1 if the channel type does not support meter subscription (e.g. DCA)
    int32_t meterChannelId(const TriggerConfig& t) const;

    // Direct access to the config struct
    DeviceConfig&       cfg()       { return _cfg; }
    const DeviceConfig& cfg() const { return _cfg; }

private:
    ConfigManager() {}
    void applyDefaults();
    DeviceConfig _cfg;
};

// Convenience global accessor
#define Config ConfigManager::instance().cfg()
