#pragma once
// =============================================================================
// MixerConnection.h  –  OSC/UDP link to X32 / M32 mixer
// =============================================================================

#include <Arduino.h>
#include <WiFiUdp.h>
#include "OSCHandler.h"

class MixerConnection {
public:
    static MixerConnection& instance() {
        static MixerConnection inst;
        return inst;
    }

    void begin();
    void loop();

    // Trigger an immediate re-subscribe / poll cycle
    void reconnect();

    // ── Status ───────────────────────────────────────────────────────────────
    bool  isConnected()   const { return _connected; }
    float getCurrentLevel() const { return _currentLevel; }

private:
    MixerConnection() {}

    void sendQuery();
    void sendXRemote();
    void processIncoming();

    WiFiUdp  _udp;
    bool     _udpOpen      = false;
    bool     _connected    = false;
    float    _currentLevel = 0.0f;

    uint32_t _lastPollMs        = 0;
    uint32_t _lastXRemoteMs     = 0;
    uint32_t _lastResponseMs    = 0;
    uint32_t _lastReconnectMs   = 0;

    uint8_t  _txBuf[256];
    uint8_t  _rxBuf[1024];
};
