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

    // True if this meter trigger wanted the postfader /meters/6 source but was
    // blocked because another trigger already owns the single /meters/6 slot.
    bool isMeterBlocked(uint8_t n) const {
        return (n < MAX_TRIGGERS) ? _meterBlocked[n] : false;
    }

private:
    MixerConnection() {}

    void sendXRemote();
    void sendFaderMuteQueries();
    void sendMeterSubscriptions();   // (re)register the full /batchsubscribe set
    void renewMeterSubscriptions();  // lightweight /renew of all subscriptions
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
    // Postfader triggers instead use the single-channel strip /meters/6 (bank id
    // METER_BANK_M6). Per trigger we remember which bank + float index it reads.
    static const uint8_t METER_BANK_COUNT = 3;   // full banks /meters/0,1,2
    static const uint8_t METER_BANK_M6    = 3;   // _meterBank value for /meters/6
    uint8_t  _meterBank[MAX_TRIGGERS]  = {0xFF, 0xFF, 0xFF, 0xFF}; // 0xFF = none
    uint32_t _meterIndex[MAX_TRIGGERS] = {};
    bool     _meterBankUsed[METER_BANK_COUNT] = {};               // full banks to subscribe

    // /meters/6 holds the 4-float channel strip (pre/gate/dynGR/post) of ONE
    // channel — the console keeps a single global selection, so only one trigger
    // can own it. Extra postfader triggers are blocked.
    uint8_t  _m6Owner     = 0xFF;                 // trigger index owning /meters/6
    int32_t  _m6ChannelId = -1;                   // its /meters/6 channel_id (0..71)
    bool     _meterBlocked[MAX_TRIGGERS] = {};    // wanted /meters/6 but blocked

    // ── Subscription lifecycle ────────────────────────────────────────────────
    bool     _meterRegistered = false;            // batchsubscribes sent to console
    uint32_t _lastMeterRxMs   = 0;                // last meter blob received (self-heal)

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
