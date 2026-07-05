// =============================================================================
// StorageManager.cpp  –  NVS Preferences wrappers
// =============================================================================

#include "StorageManager.h"
#include "config.h"
#include <Arduino.h>

// Open/close the NVS namespace (read-write, read-only, close).
bool StorageManager::openRW() { return _prefs.begin(NVS_NAMESPACE, false); }
bool StorageManager::openRO() { return _prefs.begin(NVS_NAMESPACE, true);  }
void StorageManager::close()  { _prefs.end(); }

// ─────────────────────────────────────────────────────────────────────────────

// Write every field of the config to NVS under short per-field keys.
bool StorageManager::save(const DeviceConfig& c) {
    if (!openRW()) return false;

    // Network
    _prefs.putString("ap_pw",     c.apPassword);
    _prefs.putString("wifi_ssid", c.wifiSSID);
    _prefs.putString("wifi_pw",   c.wifiPassword);
    _prefs.putBool  ("use_dhcp",  c.useDHCP);
    _prefs.putString("s_ip",      c.staticIP);
    _prefs.putString("s_gw",      c.staticGateway);
    _prefs.putString("s_sn",      c.staticSubnet);

    // Mixer connection
    _prefs.putString("mix_ip",  c.mixerIP);
    _prefs.putUShort("osc_tx",  c.oscTxPort);
    _prefs.putUShort("osc_rx",  c.oscRxPort);

    // Triggers — per-trigger keys: t{n}_<field>  (all ≤ 15 chars)
    for (uint8_t n = 0; n < MAX_TRIGGERS; n++) {
        const TriggerConfig& t = c.triggers[n];
        char key[16];

        snprintf(key, sizeof(key), "t%u_en",   n); _prefs.putBool  (key, t.enabled);
        snprintf(key, sizeof(key), "t%u_ct",   n); _prefs.putUChar (key, t.channelType);
        snprintf(key, sizeof(key), "t%u_cn",   n); _prefs.putUChar (key, t.channelNumber);
        snprintf(key, sizeof(key), "t%u_ss",   n); _prefs.putUChar (key, t.signalSource);
        snprintf(key, sizeof(key), "t%u_mpf",  n); _prefs.putULong  (key, t.meterSignalType);
        snprintf(key, sizeof(key), "t%u_pth",  n); _prefs.putString(key, t.customOSCPath);
        snprintf(key, sizeof(key), "t%u_thr",  n); _prefs.putFloat (key, t.threshold);
        snprintf(key, sizeof(key), "t%u_hyst", n); _prefs.putFloat (key, t.hysteresis);
        snprintf(key, sizeof(key), "t%u_smth", n); _prefs.putFloat (key, t.smoothing);
        snprintf(key, sizeof(key), "t%u_hld",  n); _prefs.putULong (key, t.holdTimeMs);
        snprintf(key, sizeof(key), "t%u_rel",  n); _prefs.putULong (key, t.releaseDelayMs);
        snprintf(key, sizeof(key), "t%u_dbnc", n); _prefs.putULong (key, t.debounceMs);
        snprintf(key, sizeof(key), "t%u_inv",  n); _prefs.putBool  (key, t.invert);
        snprintf(key, sizeof(key), "t%u_lcc",  n); _prefs.putBool  (key, t.useTriggerColor);
        snprintf(key, sizeof(key), "t%u_lr",   n); _prefs.putUChar (key, t.trigLedR);
        snprintf(key, sizeof(key), "t%u_lg",   n); _prefs.putUChar (key, t.trigLedG);
        snprintf(key, sizeof(key), "t%u_lb",   n); _prefs.putUChar (key, t.trigLedB);
        snprintf(key, sizeof(key), "t%u_on",   n); _prefs.putString(key, t.onJson);
        snprintf(key, sizeof(key), "t%u_off",  n); _prefs.putString(key, t.offJson);
    }

    // Output
    _prefs.putUChar ("out_type",   c.outputType);
    _prefs.putUChar ("out_pin",    c.outputPin);
    _prefs.putBool  ("out_inv",    c.outputInvert);
    _prefs.putUChar ("flash_mode", c.flashMode);
    _prefs.putUShort("flash_spd",  c.flashSpeedMs);

    // LED
    _prefs.putUChar ("led_pin",  c.ledPin);
    _prefs.putUShort("led_cnt",  c.ledCount);
    _prefs.putUChar ("led_bri",  c.ledBrightness);
    _prefs.putUChar ("led_r",    c.ledR);
    _prefs.putUChar ("led_g",    c.ledG);
    _prefs.putUChar ("led_b",    c.ledB);

    // Talkback Engine
    _prefs.putBool  ("tb_en",    c.tbEnabled);
    _prefs.putUChar ("tb_mon",   c.tbMonitor);
    _prefs.putString("tb_a_on",  c.tbAOnJson);
    _prefs.putString("tb_a_off", c.tbAOffJson);
    _prefs.putString("tb_b_on",  c.tbBOnJson);
    _prefs.putString("tb_b_off", c.tbBOffJson);

    // External OSC
    _prefs.putBool  ("xosc_en",   c.extOscEnabled);
    _prefs.putUShort("xosc_port", c.extOscPort);

    _prefs.putBool("initialised", true);
    close();
    return true;
}

// Load the config from NVS, keeping each field's default if its key is missing.
// Returns false if nothing has ever been stored (the "initialised" flag absent).
bool StorageManager::load(DeviceConfig& c) {
    if (!openRO()) return false;

    if (!_prefs.getBool("initialised", false)) {
        close();
        return false;
    }

    // Network
    strlcpy(c.apPassword,    _prefs.getString("ap_pw",     c.apPassword).c_str(),    sizeof(c.apPassword));
    strlcpy(c.wifiSSID,      _prefs.getString("wifi_ssid", c.wifiSSID).c_str(),      sizeof(c.wifiSSID));
    strlcpy(c.wifiPassword,  _prefs.getString("wifi_pw",   c.wifiPassword).c_str(),  sizeof(c.wifiPassword));
    c.useDHCP = _prefs.getBool("use_dhcp", c.useDHCP);
    strlcpy(c.staticIP,      _prefs.getString("s_ip", c.staticIP).c_str(),           sizeof(c.staticIP));
    strlcpy(c.staticGateway, _prefs.getString("s_gw", c.staticGateway).c_str(),      sizeof(c.staticGateway));
    strlcpy(c.staticSubnet,  _prefs.getString("s_sn", c.staticSubnet).c_str(),       sizeof(c.staticSubnet));

    // Mixer
    strlcpy(c.mixerIP, _prefs.getString("mix_ip", c.mixerIP).c_str(), sizeof(c.mixerIP));
    c.oscTxPort = _prefs.getUShort("osc_tx", c.oscTxPort);
    c.oscRxPort = _prefs.getUShort("osc_rx", c.oscRxPort);

    // Triggers
    for (uint8_t n = 0; n < MAX_TRIGGERS; n++) {
        TriggerConfig& t = c.triggers[n];
        char key[16];

        snprintf(key, sizeof(key), "t%u_en",   n); t.enabled       = _prefs.getBool  (key, t.enabled);
        snprintf(key, sizeof(key), "t%u_ct",   n); t.channelType   = _prefs.getUChar (key, t.channelType);
        snprintf(key, sizeof(key), "t%u_cn",   n); t.channelNumber = _prefs.getUChar (key, t.channelNumber);
        snprintf(key, sizeof(key), "t%u_ss",   n); t.signalSource  = _prefs.getUChar (key, t.signalSource);
        snprintf(key, sizeof(key), "t%u_mpf",  n); t.meterSignalType = _prefs.getULong  (key, t.meterSignalType);
        snprintf(key, sizeof(key), "t%u_pth",  n); strlcpy(t.customOSCPath, _prefs.getString(key, t.customOSCPath).c_str(), sizeof(t.customOSCPath));
        snprintf(key, sizeof(key), "t%u_thr",  n); t.threshold     = _prefs.getFloat (key, t.threshold);
        snprintf(key, sizeof(key), "t%u_hyst", n); t.hysteresis    = _prefs.getFloat (key, t.hysteresis);
        snprintf(key, sizeof(key), "t%u_smth", n); t.smoothing     = _prefs.getFloat (key, t.smoothing);
        snprintf(key, sizeof(key), "t%u_hld",  n); t.holdTimeMs    = _prefs.getULong (key, t.holdTimeMs);
        snprintf(key, sizeof(key), "t%u_rel",  n); t.releaseDelayMs= _prefs.getULong (key, t.releaseDelayMs);
        snprintf(key, sizeof(key), "t%u_dbnc", n); t.debounceMs    = _prefs.getULong (key, t.debounceMs);
        snprintf(key, sizeof(key), "t%u_inv",  n); t.invert          = _prefs.getBool  (key, t.invert);
        snprintf(key, sizeof(key), "t%u_lcc",  n); t.useTriggerColor = _prefs.getBool  (key, t.useTriggerColor);
        snprintf(key, sizeof(key), "t%u_lr",   n); t.trigLedR        = _prefs.getUChar (key, t.trigLedR);
        snprintf(key, sizeof(key), "t%u_lg",   n); t.trigLedG        = _prefs.getUChar (key, t.trigLedG);
        snprintf(key, sizeof(key), "t%u_lb",   n); t.trigLedB        = _prefs.getUChar (key, t.trigLedB);
        snprintf(key, sizeof(key), "t%u_on",   n); strlcpy(t.onJson,  _prefs.getString(key, "").c_str(), sizeof(t.onJson));
        snprintf(key, sizeof(key), "t%u_off",  n); strlcpy(t.offJson, _prefs.getString(key, "").c_str(), sizeof(t.offJson));
    }

    // Output
    c.outputType   = _prefs.getUChar ("out_type",   c.outputType);
    c.outputPin    = _prefs.getUChar ("out_pin",    c.outputPin);
    c.outputInvert = _prefs.getBool  ("out_inv",    c.outputInvert);
    c.flashMode    = _prefs.getUChar ("flash_mode", c.flashMode);
    c.flashSpeedMs = _prefs.getUShort("flash_spd",  c.flashSpeedMs);

    // LED
    c.ledPin        = _prefs.getUChar ("led_pin",  c.ledPin);
    c.ledCount      = _prefs.getUShort("led_cnt",  c.ledCount);
    c.ledBrightness = _prefs.getUChar ("led_bri",  c.ledBrightness);
    c.ledR          = _prefs.getUChar ("led_r",    c.ledR);
    c.ledG          = _prefs.getUChar ("led_g",    c.ledG);
    c.ledB          = _prefs.getUChar ("led_b",    c.ledB);

    // Talkback Engine
    c.tbEnabled = _prefs.getBool ("tb_en",  c.tbEnabled);
    c.tbMonitor = _prefs.getUChar("tb_mon", c.tbMonitor);
    strlcpy(c.tbAOnJson,  _prefs.getString("tb_a_on",  "").c_str(), sizeof(c.tbAOnJson));
    strlcpy(c.tbAOffJson, _prefs.getString("tb_a_off", "").c_str(), sizeof(c.tbAOffJson));
    strlcpy(c.tbBOnJson,  _prefs.getString("tb_b_on",  "").c_str(), sizeof(c.tbBOnJson));
    strlcpy(c.tbBOffJson, _prefs.getString("tb_b_off", "").c_str(), sizeof(c.tbBOffJson));

    // External OSC
    c.extOscEnabled = _prefs.getBool  ("xosc_en",   c.extOscEnabled);
    c.extOscPort    = _prefs.getUShort("xosc_port", c.extOscPort);

    close();
    return true;
}

// Wipe all stored keys in the namespace (factory reset).
bool StorageManager::erase() {
    if (!openRW()) return false;
    bool ok = _prefs.clear();
    close();
    return ok;
}
