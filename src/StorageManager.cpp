// =============================================================================
// StorageManager.cpp  –  NVS Preferences wrappers
// =============================================================================

#include "StorageManager.h"
#include "config.h"
#include <Arduino.h>

// ── Open helpers ─────────────────────────────────────────────────────────────

bool StorageManager::openRW() {
    return _prefs.begin(NVS_NAMESPACE, false);
}
bool StorageManager::openRO() {
    return _prefs.begin(NVS_NAMESPACE, true);
}
void StorageManager::close() {
    _prefs.end();
}

// ── Public API ───────────────────────────────────────────────────────────────

bool StorageManager::save(const DeviceConfig& c) {
    if (!openRW()) return false;

    // Network
    _prefs.putString("ap_pw",      c.apPassword);
    _prefs.putString("wifi_ssid",  c.wifiSSID);
    _prefs.putString("wifi_pw",    c.wifiPassword);
    _prefs.putBool  ("use_dhcp",   c.useDHCP);
    _prefs.putString("s_ip",       c.staticIP);
    _prefs.putString("s_gw",       c.staticGateway);
    _prefs.putString("s_sn",       c.staticSubnet);

    // Mixer
    _prefs.putString("mix_ip",     c.mixerIP);
    _prefs.putUShort("osc_tx",     c.oscTxPort);
    _prefs.putUShort("osc_rx",     c.oscRxPort);
    _prefs.putUChar ("mix_type",   c.mixerType);
    _prefs.putUChar ("ch_type",    c.channelType);
    _prefs.putUChar ("ch_num",     c.channelNumber);
    _prefs.putUChar ("sig_src",    c.signalSource);
    _prefs.putString("osc_path",   c.customOSCPath);

    // Trigger
    _prefs.putFloat ("thresh",     c.threshold);
    _prefs.putULong ("hold_ms",    c.holdTimeMs);
    _prefs.putULong ("rel_ms",     c.releaseDelayMs);
    _prefs.putFloat ("hyst",       c.hysteresis);
    _prefs.putFloat ("smooth",     c.smoothing);
    _prefs.putULong ("dbnc_ms",    c.debounceMs);

    // Output
    _prefs.putUChar ("out_type",   c.outputType);
    _prefs.putUChar ("out_pin",    c.outputPin);
    _prefs.putBool  ("out_inv",    c.outputInvert);
    _prefs.putUChar ("flash_mode", c.flashMode);
    _prefs.putUShort("flash_spd",  c.flashSpeedMs);

    // Talkback Engine
    _prefs.putBool  ("tb_en",      c.tbEnabled);
    _prefs.putUChar ("tb_mon",     c.tbMonitor);
    _prefs.putBool  ("tb_clrsolo", c.tbClearSolo);
    _prefs.putBool  ("tb_solo_en", c.tbSoloEnabled);
    _prefs.putUChar ("tb_solo_t",  c.tbSoloType);
    _prefs.putUChar ("tb_solo_n",  c.tbSoloNumber);
    _prefs.putString("tb_on1",     c.tbOnCmd1);
    _prefs.putString("tb_on2",     c.tbOnCmd2);
    _prefs.putString("tb_off1",    c.tbOffCmd1);
    _prefs.putString("tb_off2",    c.tbOffCmd2);

    // LED
    _prefs.putUChar ("led_pin",    c.ledPin);
    _prefs.putUShort("led_cnt",    c.ledCount);
    _prefs.putUChar ("led_bri",    c.ledBrightness);
    _prefs.putUChar ("led_r",      c.ledR);
    _prefs.putUChar ("led_g",      c.ledG);
    _prefs.putUChar ("led_b",      c.ledB);

    // Marker so we know data is valid on next boot
    _prefs.putBool("initialised", true);

    close();
    return true;
}

bool StorageManager::load(DeviceConfig& c) {
    if (!openRO()) return false;

    // Check whether the namespace was ever written
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
    strlcpy(c.mixerIP,       _prefs.getString("mix_ip",  c.mixerIP).c_str(),         sizeof(c.mixerIP));
    c.oscTxPort     = _prefs.getUShort("osc_tx",     c.oscTxPort);
    c.oscRxPort     = _prefs.getUShort("osc_rx",     c.oscRxPort);
    c.mixerType     = _prefs.getUChar ("mix_type",   c.mixerType);
    c.channelType   = _prefs.getUChar ("ch_type",    c.channelType);
    c.channelNumber = _prefs.getUChar ("ch_num",     c.channelNumber);
    c.signalSource  = _prefs.getUChar ("sig_src",    c.signalSource);
    strlcpy(c.customOSCPath, _prefs.getString("osc_path", c.customOSCPath).c_str(),  sizeof(c.customOSCPath));

    // Trigger
    c.threshold      = _prefs.getFloat ("thresh",     c.threshold);
    c.holdTimeMs     = _prefs.getULong ("hold_ms",    c.holdTimeMs);
    c.releaseDelayMs = _prefs.getULong ("rel_ms",     c.releaseDelayMs);
    c.hysteresis     = _prefs.getFloat ("hyst",       c.hysteresis);
    c.smoothing      = _prefs.getFloat ("smooth",     c.smoothing);
    c.debounceMs     = _prefs.getULong ("dbnc_ms",    c.debounceMs);

    // Output
    c.outputType    = _prefs.getUChar ("out_type",   c.outputType);
    c.outputPin     = _prefs.getUChar ("out_pin",    c.outputPin);
    c.outputInvert  = _prefs.getBool  ("out_inv",    c.outputInvert);
    c.flashMode     = _prefs.getUChar ("flash_mode", c.flashMode);
    c.flashSpeedMs  = _prefs.getUShort("flash_spd",  c.flashSpeedMs);

    // Talkback Engine
    c.tbEnabled     = _prefs.getBool  ("tb_en",      c.tbEnabled);
    c.tbMonitor     = _prefs.getUChar ("tb_mon",     c.tbMonitor);
    c.tbClearSolo   = _prefs.getBool  ("tb_clrsolo", c.tbClearSolo);
    c.tbSoloEnabled = _prefs.getBool  ("tb_solo_en", c.tbSoloEnabled);
    c.tbSoloType    = _prefs.getUChar ("tb_solo_t",  c.tbSoloType);
    c.tbSoloNumber  = _prefs.getUChar ("tb_solo_n",  c.tbSoloNumber);
    strlcpy(c.tbOnCmd1,  _prefs.getString("tb_on1",  c.tbOnCmd1).c_str(),  sizeof(c.tbOnCmd1));
    strlcpy(c.tbOnCmd2,  _prefs.getString("tb_on2",  c.tbOnCmd2).c_str(),  sizeof(c.tbOnCmd2));
    strlcpy(c.tbOffCmd1, _prefs.getString("tb_off1", c.tbOffCmd1).c_str(), sizeof(c.tbOffCmd1));
    strlcpy(c.tbOffCmd2, _prefs.getString("tb_off2", c.tbOffCmd2).c_str(), sizeof(c.tbOffCmd2));

    // LED
    c.ledPin        = _prefs.getUChar ("led_pin",    c.ledPin);
    c.ledCount      = _prefs.getUShort("led_cnt",    c.ledCount);
    c.ledBrightness = _prefs.getUChar ("led_bri",    c.ledBrightness);
    c.ledR          = _prefs.getUChar ("led_r",      c.ledR);
    c.ledG          = _prefs.getUChar ("led_g",      c.ledG);
    c.ledB          = _prefs.getUChar ("led_b",      c.ledB);

    close();
    return true;
}

bool StorageManager::erase() {
    if (!openRW()) return false;
    bool ok = _prefs.clear();
    close();
    return ok;
}
