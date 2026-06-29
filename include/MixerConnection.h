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

    // True if this meter trigger asked for the post-fader (/meters/6) source but
    // was blocked because another trigger already holds the single /meters/6
    // channel selection on a different channel.
    bool isMeterBlocked(uint8_t n) const {
        return (n < MAX_TRIGGERS) ? _meterBlocked[n] : false;
    }

private:
    MixerConnection() {}

    void sendXRemote();
    void sendFaderMuteQueries();
    void sendMeterSubscriptions();   // (re)register/renew bulk banks + /meters/6 select
    void unsubscribeAllMeters();     // /unsubscribe — stop all subscriptions
    void processIncoming();

    // Rebuild _triggerPaths[] from current config; called at begin/reconnect
    void rebuildPaths();

    WiFiUDP  _udp;
    bool     _udpOpen   = false;
    bool     _connected = false;

    float    _triggerLevels[MAX_TRIGGERS] = {};
    // Resolved path for FADER/MUTE triggers (response matching). Empty for METER.
    String   _triggerPaths[MAX_TRIGGERS];

    // ── Meter-bank routing (SIG_METER triggers) ───────────────────────────────
    // Most meter triggers share up to 3 full-bank subscriptions (/meters/0,1,2).
    // Post-fader (and Main pre) read from /meters/6 (bank id METER_BANK_M6).
    static const uint8_t METER_BANK_COUNT = 3;   // full banks /meters/0,1,2
    static const uint8_t METER_BANK_M6    = 3;   // _meterBank value for /meters/6
    uint8_t  _meterBank[MAX_TRIGGERS]  = {0xFF, 0xFF, 0xFF, 0xFF}; // 0xFF = none
    uint32_t _meterIndex[MAX_TRIGGERS] = {};      // float index in bank, or tap in /m6
    bool     _meterBankUsed[METER_BANK_COUNT] = {};     // full banks to subscribe

    // /meters/6 has ONE console-wide channel selection: only one channel can be
    // metered via the strip at a time. The first /meters/6 trigger claims the
    // channel; other /meters/6 triggers on a DIFFERENT channel are blocked.
    // (Several triggers on the SAME channel — e.g. different taps — all work.)
    int16_t  _m6ChannelId = -1;                   // the one active /meters/6 channel
    bool     _meterBlocked[MAX_TRIGGERS] = {};    // wanted /meters/6 but channel taken

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
