// =============================================================================
// ConfigManager.cpp
// =============================================================================

#include "ConfigManager.h"
#include "StorageManager.h"
#include <Arduino.h>

void ConfigManager::begin() {
    applyDefaults();
    // Overlay with any stored values; silently continue on first boot
    StorageManager::instance().load(_cfg);
    Serial.println("[Config] Configuration loaded.");
}

bool ConfigManager::save() {
    bool ok = StorageManager::instance().save(_cfg);
    Serial.println(ok ? "[Config] Saved." : "[Config] Save FAILED!");
    return ok;
}

void ConfigManager::resetToDefaults() {
    StorageManager::instance().erase();
    applyDefaults();
    Serial.println("[Config] Reset to defaults.");
}

void ConfigManager::applyDefaults() {
    // Network
    strlcpy(_cfg.apPassword,      DEFAULT_AP_PASSWORD,  sizeof(_cfg.apPassword));
    _cfg.wifiSSID[0]     = '\0';
    _cfg.wifiPassword[0] = '\0';
    _cfg.useDHCP         = true;
    strlcpy(_cfg.staticIP,        "192.168.1.100",      sizeof(_cfg.staticIP));
    strlcpy(_cfg.staticGateway,   "192.168.1.1",        sizeof(_cfg.staticGateway));
    strlcpy(_cfg.staticSubnet,    "255.255.255.0",      sizeof(_cfg.staticSubnet));

    // Mixer
    strlcpy(_cfg.mixerIP,         DEFAULT_MIXER_IP,     sizeof(_cfg.mixerIP));
    _cfg.oscTxPort      = DEFAULT_OSC_TX_PORT;
    _cfg.oscRxPort      = DEFAULT_OSC_RX_PORT;
    _cfg.mixerType      = MIXER_X32;
    _cfg.channelType    = CH_DCA;
    _cfg.channelNumber  = 1;
    _cfg.signalSource   = SIG_FADER;
    _cfg.customOSCPath[0] = '\0';

    // Trigger
    _cfg.threshold       = DEFAULT_THRESHOLD;
    _cfg.holdTimeMs      = DEFAULT_HOLD_TIME_MS;
    _cfg.releaseDelayMs  = DEFAULT_RELEASE_DELAY_MS;
    _cfg.hysteresis      = DEFAULT_HYSTERESIS;
    _cfg.smoothing       = DEFAULT_SMOOTHING;
    _cfg.debounceMs      = DEFAULT_DEBOUNCE_MS;

    // Output
    _cfg.outputType      = OUTPUT_BOTH;
    _cfg.outputPin       = DEFAULT_OUTPUT_PIN;
    _cfg.outputInvert    = false;
    _cfg.flashMode       = DEFAULT_FLASH_MODE;
    _cfg.flashSpeedMs    = DEFAULT_FLASH_SPEED_MS;

    // Talkback Engine
    _cfg.tbEnabled       = false;
    _cfg.tbMonitor       = TB_MONITOR_A;
    _cfg.tbAOnJson[0]    = '\0';
    _cfg.tbAOffJson[0]   = '\0';
    _cfg.tbBOnJson[0]    = '\0';
    _cfg.tbBOffJson[0]   = '\0';

    // LED
    _cfg.ledPin          = DEFAULT_LED_DATA_PIN;
    _cfg.ledCount        = DEFAULT_LED_COUNT;
    _cfg.ledBrightness   = DEFAULT_LED_BRIGHTNESS;
    _cfg.ledR            = DEFAULT_LED_R;
    _cfg.ledG            = DEFAULT_LED_G;
    _cfg.ledB            = DEFAULT_LED_B;
}

String ConfigManager::buildOSCPath() const {
    // Custom path takes priority
    if (_cfg.customOSCPath[0] != '\0') {
        return String(_cfg.customOSCPath);
    }

    char ch[8];
    uint8_t n = _cfg.channelNumber;

    // Build base path for the channel type
    String base;
    switch (_cfg.channelType) {
        case CH_INPUT:
            snprintf(ch, sizeof(ch), "%02d", n);
            base = String("/ch/") + ch;
            break;
        case CH_BUS:
            snprintf(ch, sizeof(ch), "%02d", n);
            base = String("/bus/") + ch;
            break;
        case CH_MATRIX:
            snprintf(ch, sizeof(ch), "%02d", n);
            base = String("/mtx/") + ch;
            break;
        case CH_DCA:
            // DCA uses single digit, no leading zero
            base = String("/dca/") + n;
            break;
        default:
            base = "/ch/01";
    }

    // Append signal-specific suffix
    switch (_cfg.signalSource) {
        case SIG_FADER: return base + "/mix/fader";
        case SIG_MUTE:  return base + "/mix/on";
        case SIG_METER: return "/meters/0";  // subscribe to meter bus 0
        default:        return base + "/mix/fader";
    }
}
