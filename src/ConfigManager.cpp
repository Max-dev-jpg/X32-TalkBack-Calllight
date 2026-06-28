// =============================================================================
// ConfigManager.cpp
// =============================================================================

#include "ConfigManager.h"
#include "StorageManager.h"
#include <Arduino.h>

void ConfigManager::begin()
{
    applyDefaults();
    StorageManager::instance().load(_cfg);
    DBG_PRINTLN("[Config] Configuration loaded.");
}

bool ConfigManager::save()
{
    bool ok = StorageManager::instance().save(_cfg);
    DBG_PRINTLN(ok ? "[Config] Saved." : "[Config] Save FAILED!");
    return ok;
}

void ConfigManager::resetToDefaults()
{
    StorageManager::instance().erase();
    applyDefaults();
    DBG_PRINTLN("[Config] Reset to defaults.");
}

void ConfigManager::applyDefaults()
{
    // Network
    strlcpy(_cfg.apPassword, DEFAULT_AP_PASSWORD, sizeof(_cfg.apPassword));
    _cfg.wifiSSID[0]    = '\0';
    _cfg.wifiPassword[0] = '\0';
    _cfg.useDHCP = true;
    strlcpy(_cfg.staticIP,      "192.168.1.100",  sizeof(_cfg.staticIP));
    strlcpy(_cfg.staticGateway, "192.168.1.1",    sizeof(_cfg.staticGateway));
    strlcpy(_cfg.staticSubnet,  "255.255.255.0",  sizeof(_cfg.staticSubnet));

    // Mixer
    strlcpy(_cfg.mixerIP, DEFAULT_MIXER_IP, sizeof(_cfg.mixerIP));
    _cfg.oscTxPort = DEFAULT_OSC_TX_PORT;
    _cfg.oscRxPort = DEFAULT_OSC_RX_PORT;

    // Triggers — trigger 0 enabled by default, others disabled
    for (uint8_t n = 0; n < MAX_TRIGGERS; n++) {
        TriggerConfig& t = _cfg.triggers[n];
        t.enabled         = (n == 0);        // only trigger 0 on by default
        t.channelType     = CH_DCA;
        t.channelNumber   = 1;
        t.signalSource    = SIG_FADER;
        t.meterSignalType = 0;        // default: pre-fader meter tap
        t.customOSCPath[0] = '\0';
        t.threshold       = DEFAULT_THRESHOLD;
        t.hysteresis      = DEFAULT_HYSTERESIS;
        t.smoothing       = DEFAULT_SMOOTHING;
        t.holdTimeMs      = DEFAULT_HOLD_TIME_MS;
        t.releaseDelayMs  = DEFAULT_RELEASE_DELAY_MS;
        t.debounceMs      = DEFAULT_DEBOUNCE_MS;
        t.invert          = false;
        t.useTriggerColor = false;
        t.trigLedR        = DEFAULT_LED_R;
        t.trigLedG        = DEFAULT_LED_G;
        t.trigLedB        = DEFAULT_LED_B;
        t.onJson[0]       = '\0';
        t.offJson[0]      = '\0';
    }

    // Output
    _cfg.outputType   = OUTPUT_BOTH;
    _cfg.outputPin    = DEFAULT_OUTPUT_PIN;
    _cfg.outputInvert = false;
    _cfg.flashMode    = DEFAULT_FLASH_MODE;
    _cfg.flashSpeedMs = DEFAULT_FLASH_SPEED_MS;

    // LED
    _cfg.ledPin        = DEFAULT_LED_DATA_PIN;
    _cfg.ledCount      = DEFAULT_LED_COUNT;
    _cfg.ledBrightness = DEFAULT_LED_BRIGHTNESS;
    _cfg.ledR          = DEFAULT_LED_R;
    _cfg.ledG          = DEFAULT_LED_G;
    _cfg.ledB          = DEFAULT_LED_B;

    // Talkback Engine
    _cfg.tbEnabled    = false;
    _cfg.tbMonitor    = TB_MONITOR_A;
    _cfg.tbBFollowsA  = false;
    _cfg.tbAOnJson[0]  = '\0';
    _cfg.tbAOffJson[0] = '\0';
    _cfg.tbBOnJson[0]  = '\0';
    _cfg.tbBOffJson[0] = '\0';

    // Multi-trigger priority
    _cfg.trigPriorityMode = PRIO_NEWEST;
    for (uint8_t n = 0; n < MAX_TRIGGERS; n++) _cfg.trigPriorityOrder[n] = n;

    // External OSC
    _cfg.extOscEnabled = false;
    _cfg.extOscPort    = DEFAULT_EXT_OSC_PORT;
}

// =============================================================================
// OSC path builder
// =============================================================================

String ConfigManager::buildOSCPathForTrigger(const TriggerConfig& t) const
{
    // Custom path takes priority
    if (t.customOSCPath[0] != '\0')
        return String(t.customOSCPath);

    // Meter: shared full-bank /batchsubscribe — display-only representation.
    // Postfader uses the single-channel /meters/6 strip (see MixerConnection).
    if (t.signalSource == SIG_METER) {
        if (t.meterSignalType == 3) {              // postfader
            int32_t ch = meterChannelId(t);
            if (ch < 0) return String("/meters/N_A");
            return String("/meters/6[ch=") + ch + " post]";
        }
        uint8_t bank; uint32_t idx;
        if (!meterBankIndex(t, bank, idx)) return String("/meters/N_A");
        return String("/meters/") + bank + "[idx=" + idx + "]";
    }

    const bool isMute = (t.signalSource == SIG_MUTE);
    const char* sfx   = isMute ? "/mix/on" : "/mix/fader";
    char        num[4];
    uint8_t     n = t.channelNumber;

    switch (t.channelType) {
        case CH_INPUT:
            snprintf(num, sizeof(num), "%02u", n);
            return String("/ch/")    + num + sfx;
        case CH_BUS:
            snprintf(num, sizeof(num), "%02u", n);
            return String("/bus/")   + num + sfx;
        case CH_MATRIX:
            snprintf(num, sizeof(num), "%02u", n);
            return String("/mtx/")   + num + sfx;
        case CH_AUXIN:
            snprintf(num, sizeof(num), "%02u", n);
            return String("/auxin/") + num + sfx;
        case CH_FXRTN:
            snprintf(num, sizeof(num), "%02u", n);
            return String("/fxrtn/") + num + sfx;
        case CH_DCA:
            if (isMute) return String("/dca/") + n + "/on";
            else        return String("/dca/") + n + "/fader";
        case CH_MAIN:
            if (isMute) return String("/main/st/mix/on");
            else        return String("/main/st/mix/fader");
        case CH_MONO:
            if (isMute) return String("/main/m/mix/on");
            else        return String("/main/m/mix/fader");
        default:
            return String("/ch/01/mix/fader");
    }
}

// =============================================================================
// /meters/6 <channelId> mapping (from the X32/M32 OSC Command Reference).
// Returns the 0-based channel id passed to /batchsubscribe for this trigger:
//   Input 1-32 → 0-31,  AuxIn 1-8 → 32-39,  FxRtn 1-8 → 40-47,
//   Bus 1-16   → 48-63, Matrix 1-6 → 64-69, Main L/R → 70, Main M/C → 71.
// DCA groups have no channel-strip meter on /meters/6 → returns -1.
// =============================================================================
int32_t ConfigManager::meterChannelId(const TriggerConfig& t) const
{
    switch (t.channelType) {
        case CH_INPUT:  return (int32_t)(t.channelNumber - 1);
        case CH_AUXIN:  return 32 + (int32_t)(t.channelNumber - 1);
        case CH_FXRTN:  return 40 + (int32_t)(t.channelNumber - 1);
        case CH_BUS:    return 48 + (int32_t)(t.channelNumber - 1);
        case CH_MATRIX: return 64 + (int32_t)(t.channelNumber - 1);
        case CH_MAIN:   return 70;
        case CH_MONO:   return 71;
        default:        return -1; // DCA and unknown types not supported
    }
}

// =============================================================================
// meterBankIndex — map a trigger to (meter bank, float index within that bank).
// Layouts from the X32/M32 OSC Command Reference (p.12-13):
//
//   /meters/0  (70 floats): 32 input | 8 auxin | 8 fxrtn | 16 bus | 6 matrix
//   /meters/1  (96 floats): 32 input level | 32 gate-GR | 32 dynamics-GR
//   /meters/2  (49 floats): 16 bus | 6 matrix | 2 main LR | 1 mono | (then GRs)
//
// Input channels use /meters/1 so the pre/gate-GR/dyn-GR tap (meterSignalType)
// stays meaningful. NOTE: /meters/1 has only ONE input level (no separate
// pre/post), so meterSignalType 0 (pre) and 3 (post) both map to that level.
// =============================================================================
bool ConfigManager::meterBankIndex(const TriggerConfig& t,
                                   uint8_t& bank, uint32_t& idx) const
{
    const uint32_t ch = (t.channelNumber > 0) ? (uint32_t)(t.channelNumber - 1) : 0;

    switch (t.channelType) {
        case CH_INPUT:
            bank = 1;
            switch (t.meterSignalType) {
                case 1:  idx = 32 + ch; break;   // gate gain reduction
                case 2:  idx = 64 + ch; break;   // dynamics gain reduction
                default: idx =      ch; break;   // pre/post → input channel level
            }
            return true;
        case CH_AUXIN:  bank = 0; idx = 32 + ch; return true;  // /meters/0
        case CH_FXRTN:  bank = 0; idx = 40 + ch; return true;
        case CH_BUS:    bank = 0; idx = 48 + ch; return true;
        case CH_MATRIX: bank = 0; idx = 64 + ch; return true;
        case CH_MAIN:   bank = 2; idx = 22;      return true;  // /meters/2 Main L
        case CH_MONO:   bank = 2; idx = 24;      return true;  // /meters/2 Mono M/C
        default:        return false;                          // DCA unsupported
    }
}
