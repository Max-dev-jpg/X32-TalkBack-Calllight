#pragma once
// =============================================================================
// MixerConnection.h  –  OSC/UDP link to X32 / M32 mixer
//
// Handles up to MAX_TRIGGERS independent signal paths simultaneously:
//   - SIG_FADER / SIG_MUTE : one-shot query on /xremote renewal (every 8 s),
//                             then relies on /xremote push for real-time changes
//   - SIG_METER             : subscribed via /batchsubscribe, renewed every 8 s
// =============================================================================

#include <Arduino.h>
#include <WiFiUdp.h>
#include "OSCHandler.h"
#include "config.h"

class MixerConnection {
public:
    static MixerConnection& instance() {
        static MixerConnection inst;
        return inst;
    }

    void begin();
    void loop();
    void reconnect();

    // ── Per-trigger levels ────────────────────────────────────────────────────
    float getLevelForTrigger(uint8_t n) const {
        return (n < MAX_TRIGGERS) ? _triggerLevels[n] : 0.0f;
    }
    // Backward-compat: level for trigger 0
    float getCurrentLevel() const { return _triggerLevels[0]; }

    bool isConnected() const { return _connected; }

private:
    MixerConnection() {}

    void sendXRemote();
    void sendFaderMuteQueries();
    void sendMeterSubscriptions();
    void processIncoming();

    // Rebuild _triggerPaths[] from current config; called at begin/reconnect
    void rebuildPaths();

    WiFiUDP  _udp;
    bool     _udpOpen   = false;
    bool     _connected = false;

    float    _triggerLevels[MAX_TRIGGERS] = {};
    // Resolved path or alias for each trigger (used for response matching)
    String   _triggerPaths[MAX_TRIGGERS];
    // 0-based channel index in the /meters/6 blob for SIG_METER triggers; -1 otherwise
    int32_t  _meterChannelIds[MAX_TRIGGERS] = {-1,-1,-1,-1};

    uint32_t _lastXRemoteMs      = 0;
    uint32_t _lastResponseMs     = 0;
    uint32_t _lastReconnectMs    = 0;
    uint32_t _lastKeepaliveMs    = 0;
    uint32_t _lastIgnoredPacketMs = 0;
    uint32_t _lastInvalidPacketMs = 0;
    uint32_t _lastBlobDebugMs[MAX_TRIGGERS] = {}; // throttle: blob debug once per 2 s per trigger

    uint8_t  _txBuf[256];
    uint8_t  _rxBuf[1024];
};
