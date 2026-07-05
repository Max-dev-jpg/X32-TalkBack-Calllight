#pragma once
// =============================================================================
// NetworkManager.h  –  WiFi AP+STA, mDNS, OTA
// =============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>

class NetworkManager {

    struct NetworkConfigSnapshot {
    char ssid[33];
    char password[64];
    bool useDHCP;
    char staticIP[16];
    };

public:
    // Singleton accessor.
    static NetworkManager& instance() {
        static NetworkManager inst;
        return inst;
    }

    // Call once in setup()
    void begin();

    // Call every loop() iteration
    void loop();

    // ── Status accessors ─────────────────────────────────────────────────────
    bool   isSTAConnected()  const { return _staConnected; }
    int8_t getRSSI()         const { return _rssi; }
    uint8_t getAPClientCount() const;
    String  getSTAIP()       const { return WiFi.localIP().toString(); }
    String  getAPIP()        const { return WiFi.softAPIP().toString(); }

    // Re-apply Wi-Fi settings after a config change (reconnects only if changed).
    void applyConfig();

private:
    NetworkManager() {}

    void startAP();    // bring up the fallback Access Point (AP+STA mode)
    void startSTA();   // connect to the configured Wi-Fi network as a station
    void setupMDNS();  // advertise the device as <hostname>.local
    void setupOTA();   // enable ArduinoOTA firmware updates

    bool     _staConnected        = false;
    bool     _otaInitialised      = false;
    int8_t   _rssi                = 0;
    uint32_t _lastReconnectAttempt = 0;
    uint32_t _lastRSSIUpdate       = 0;

    NetworkConfigSnapshot _lastConfig;
    bool hasConfigChanged();   // true if Wi-Fi/IP settings differ from _lastConfig
};
