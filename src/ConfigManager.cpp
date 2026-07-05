// =============================================================================
// ConfigManager.cpp
// =============================================================================

#include "ConfigManager.h"
#include "StorageManager.h"
#include <Arduino.h>

// Load defaults, then overlay whatever is stored in NVS (stored values win).
void ConfigManager::begin()
{
    applyDefaults();
    StorageManager::instance().load(_cfg);
    DBG_PRINTLN("[Config] Configuration loaded.");
}

// Persist the current config to NVS.
bool ConfigManager::save()
{
    bool ok = StorageManager::instance().save(_cfg);
    DBG_PRINTLN(ok ? "[Config] Saved." : "[Config] Save FAILED!");
    return ok;
}

// Erase NVS and restore compiled-in defaults (factory reset).
void ConfigManager::resetToDefaults()
{
    StorageManager::instance().erase();
    applyDefaults();
    DBG_PRINTLN("[Config] Reset to defaults.");
}

// Populate _cfg with the compiled-in factory defaults from config.h.
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

    // Meter: display-only representation of the resolved bank + index.
    if (t.signalSource == SIG_METER) {
        uint8_t bank; uint32_t idx;
        if (!meterRoute(t, bank, idx)) return String("/meters/N_A");
        static const char* TAP[4] = { "pre", "gate", "comp", "post" };
        const char* tapStr = (t.meterSignalType < 4) ? TAP[t.meterSignalType] : "?";
        if (bank == METER_ROUTE_M6) {              // /meters/6 scanner channel
            return String("/meters/6[ch=") + meterChannelId(t) + " " + tapStr + "]";
        }
        return String("/meters/") + bank + "[idx=" + idx + " " + tapStr + "]";
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
// meterRoute — map (channel type, tap) to (meter bank, float index).
// Bank tap nature CONFIRMED live on the console:
//
//   /meters/0 (70): 32 input | 8 auxin | 8 fxrtn | 16 bus | 6 matrix  — PRE
//   /meters/1 (96): 32 input PRE [0..31] | 32 gate-GR [32..63] | 32 comp-GR [64..95]
//   /meters/2 (49): bus PRE [0..15] | mtx PRE [16..21] | main L/R POST [22,23]
//                   | mono M/C POST [24] | bus-GR [25..40] | mtx-GR [41..46]
//                   | main-GR [47] | mono-GR [48]
//   /meters/6 (4) : ONE channel strip — [0]=pre [1]=gate [2]=comp [3]=post
//
// Post-fader exists in a bulk bank ONLY for Main L/R (/meters/2[22]). Every other
// post-fader value comes from /meters/6 (single channel); the console keeps one
// global /meters/6 selection, so MixerConnection lets only one such trigger use
// it. Likewise Main pre-fader is not in a bulk bank, so it also uses /meters/6.
//
// tap (meterSignalType): 0=pre, 1=gate, 2=comp, 3=post.
// =============================================================================
bool ConfigManager::meterRoute(const TriggerConfig& t,
                               uint8_t& bank, uint32_t& idx) const
{
    const uint32_t i   = (t.channelNumber > 0) ? (uint32_t)(t.channelNumber - 1) : 0;
    const uint8_t  tap = t.meterSignalType;
    const uint8_t  M6  = METER_ROUTE_M6;

    switch (t.channelType) {
        case CH_INPUT:   // /meters/1 multi-channel for pre/gate/comp; post -> /m6
            switch (tap) {
                case 0: bank = 1; idx =      i; return true;  // pre
                case 1: bank = 1; idx = 32 + i; return true;  // gate GR
                case 2: bank = 1; idx = 64 + i; return true;  // comp GR
                case 3: bank = M6; idx = 3;     return true;  // post
            }
            return false;

        case CH_AUXIN:   // /meters/0 pre level; post -> /m6
            if (tap == 3) { bank = M6; idx = 3; return true; }
            bank = 0; idx = 32 + i; return true;
        case CH_FXRTN:
            if (tap == 3) { bank = M6; idx = 3; return true; }
            bank = 0; idx = 40 + i; return true;

        case CH_BUS:     // /meters/2: pre [i], comp-GR [25+i]; post -> /m6
            if (tap == 2) { bank = 2; idx = 25 + i; return true; }
            if (tap == 3) { bank = M6; idx = 3;     return true; }
            bank = 2; idx = i; return true;                   // pre (default)
        case CH_MATRIX:  // /meters/2: pre [16+i], comp-GR [41+i]; post -> /m6
            if (tap == 2) { bank = 2; idx = 41 + i; return true; }
            if (tap == 3) { bank = M6; idx = 3;     return true; }
            bank = 2; idx = 16 + i; return true;
        case CH_MAIN:    // /meters/2: post L [22] (bulk!), comp-GR [47]; pre -> /m6
            if (tap == 2) { bank = 2; idx = 47; return true; }
            if (tap == 0) { bank = M6; idx = 0; return true; }
            bank = 2; idx = 22; return true;                  // post (default)
        case CH_MONO:    // /meters/2: post [24] (bulk!), comp-GR [48]; pre -> /m6
            if (tap == 2) { bank = 2; idx = 48; return true; }
            if (tap == 0) { bank = M6; idx = 0; return true; }
            bank = 2; idx = 24; return true;                  // post (default)

        default:        return false;                         // DCA unsupported
    }
}
