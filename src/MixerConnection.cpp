// =============================================================================
// MixerConnection.cpp  –  OSC / UDP link to X32 / M32
// =============================================================================

#include "MixerConnection.h"
#include "ConfigManager.h"
#include "OSCHandler.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>

// ── Shared meter-bank subscriptions ────────────────────────────────────────────
// One full-bank subscription per used bank; the console returns the blob with the
// alias as its OSC address. iEnd is the INCLUSIVE last float index of the bank.
namespace {
    const char* const kMeterAlias[3] = { "/m0", "/m1", "/m2" };
    const char* const kMeterPath [3] = { "/meters/0", "/meters/1", "/meters/2" };
    const int32_t     kMeterIEnd [3] = { 69, 95, 48 };  // 70 / 96 / 49 floats

    const char* const kM6Alias = "/m6";          // single /meters/6 channel strip
    const char* const kM6Path  = "/meters/6";

    // Map an incoming OSC address back to a meter-bank id: 0..2 for the full
    // banks, 3 (METER_BANK_M6) for /meters/6, or -1 if it is not a meter alias.
    int meterBankFromAlias(const String& addr) {
        for (int b = 0; b < 3; b++)
            if (addr == kMeterAlias[b]) return b;
        if (addr == kM6Alias) return 3;
        return -1;
    }
}

// ─────────────────────────────────────────────────────────────────────────────

void MixerConnection::begin() {
    DBG_PRINTLN("[Mixer] Initialising UDP...");
    if (_udpOpen) {
        _udp.stop();
        _udpOpen = false;
    }
    if (_udp.begin(Config.oscRxPort)) {
        _udpOpen = true;
        DBG_PRINTF("[Mixer] Listening on UDP port %d\n", Config.oscRxPort);
    } else {
        DBG_PRINTLN("[Mixer] UDP begin FAILED!");
    }

    rebuildPaths();

    if (_udpOpen) {
        // Perform an immediate handshake/query so the mixer starts pushing updates
        // and the connected flag can be confirmed without waiting for the first loop.
        sendXRemote();
        sendMeterSubscriptions();   // register the fresh meter set
        sendFaderMuteQueries();
        uint32_t now = millis();
        _meterRegistered  = true;
        _lastMeterRxMs    = now;
        _lastXRemoteMs    = now;
        _lastKeepaliveMs  = now;
        _lastReconnectMs  = now;
        _lastResponseMs   = now;
    }
}

void MixerConnection::reconnect() {
    DBG_PRINTLN("[Mixer] Reconnecting...");
    if (_udpOpen) {
        unsubscribeAllMeters();      // drop stale subs (e.g. old /meters/6 channel)
        _udp.stop();
        _udpOpen = false;
    }
    _lastXRemoteMs   = 0;   // force immediate xremote + queries on next loop()
    _lastResponseMs  = 0;
    _connected       = false;
    _meterRegistered = false;
    begin();
}

// ── Rebuild resolved path / alias strings ─────────────────────────────────────

void MixerConnection::rebuildPaths() {
    for (uint8_t b = 0; b < METER_BANK_COUNT; b++) _meterBankUsed[b] = false;
    _m6Owner     = 0xFF;
    _m6ChannelId = -1;

    for (uint8_t n = 0; n < MAX_TRIGGERS; n++) {
        const TriggerConfig& t = Config.triggers[n];
        _meterBank[n]    = 0xFF;
        _meterIndex[n]   = 0;
        _meterBlocked[n] = false;
        if (!t.enabled) {
            _triggerPaths[n] = "";
            continue;
        }
        if (t.signalSource == SIG_METER) {
            // Meters use no _triggerPaths entry; the fader/mute matcher ignores
            // them. Postfader (meterSignalType == 3) is only available from the
            // single-channel strip /meters/6 — at most one trigger can own it.
            // Everything else reads from the shared full banks /meters/0,1,2.
            _triggerPaths[n] = "";
            const bool wantsPost = (t.meterSignalType == 3);

            if (wantsPost) {
                int32_t chId = ConfigManager::instance().meterChannelId(t);
                if (chId < 0) {
                    // e.g. DCA — no meter at all
                } else if (_m6Owner == 0xFF) {
                    _m6Owner       = n;
                    _m6ChannelId   = chId;
                    _meterBank[n]  = METER_BANK_M6;
                    _meterIndex[n] = 3;            // post-fade float in the strip
                } else {
                    _meterBlocked[n] = true;       // /meters/6 already taken
                }
            } else {
                uint8_t bank; uint32_t idx;
                if (ConfigManager::instance().meterBankIndex(t, bank, idx)) {
                    _meterBank[n]  = bank;
                    _meterIndex[n] = idx;
                    _meterBankUsed[bank] = true;
                }
            }

            // No live meter source (blocked or unsupported type): force the level
            // to 0 so a stale value can't keep the trigger active.
            if (_meterBank[n] == 0xFF) _triggerLevels[n] = 0.0f;
        } else {
            _triggerPaths[n] = ConfigManager::instance().buildOSCPathForTrigger(t);
        }
    }

    // ── Debug: print full subscription / path plan ────────────────────────────
    DBG_PRINTLN("[Mixer] ── Trigger path plan ──────────────────────────");
    for (uint8_t n = 0; n < MAX_TRIGGERS; n++) {
        const TriggerConfig& t = Config.triggers[n];
        if (!t.enabled) {
            DBG_PRINTF("[Mixer]   T%u: DISABLED\n", n);
            continue;
        }
        const char* sigStr = (t.signalSource == SIG_METER) ? "METER" :
                             (t.signalSource == SIG_MUTE)  ? "MUTE"  : "FADER";
        const char* chStr;
        switch (t.channelType) {
            case CH_INPUT:  chStr = "INPUT";   break;
            case CH_BUS:    chStr = "BUS";     break;
            case CH_MATRIX: chStr = "MATRIX";  break;
            case CH_DCA:    chStr = "DCA";     break;
            case CH_AUXIN:  chStr = "AUXIN";   break;
            case CH_FXRTN:  chStr = "FXRTN";   break;
            case CH_MAIN:   chStr = "MAIN";    break;
            case CH_MONO:   chStr = "MONO";    break;
            default:        chStr = "UNKNOWN"; break;
        }
        if (t.signalSource == SIG_METER) {
            if (_meterBank[n] == METER_BANK_M6) {
                DBG_PRINTF("[Mixer]   T%u: %-6s  ch=%-6s #%-2u  /meters/6 ch=%d idx=%u (POST)\n",
                           n, sigStr, chStr, t.channelNumber,
                           (int)_m6ChannelId, (unsigned)_meterIndex[n]);
            } else if (_meterBlocked[n]) {
                DBG_PRINTF("[Mixer]   T%u: %-6s  ch=%-6s #%-2u  BLOCKED (/meters/6 already in use)\n",
                           n, sigStr, chStr, t.channelNumber);
            } else if (_meterBank[n] != 0xFF) {
                DBG_PRINTF("[Mixer]   T%u: %-6s  ch=%-6s #%-2u  /meters/%u idx=%u\n",
                           n, sigStr, chStr, t.channelNumber,
                           _meterBank[n], (unsigned)_meterIndex[n]);
            } else {
                DBG_PRINTF("[Mixer]   T%u: %-6s  ch=%-6s #%-2u  (no meter for this type)\n",
                           n, sigStr, chStr, t.channelNumber);
            }
        } else {
            const char* custom = (t.customOSCPath[0] != '\0') ? " (custom)" : "";
            DBG_PRINTF("[Mixer]   T%u: %-6s  ch=%-6s #%-2u  path=%s%s\n",
                       n, sigStr, chStr, t.channelNumber,
                       _triggerPaths[n].c_str(), custom);
        }
    }
    DBG_PRINTF("[Mixer]   meter banks used: /meters/0=%d  /meters/1=%d  /meters/2=%d  /meters/6=%s\n",
               _meterBankUsed[0], _meterBankUsed[1], _meterBankUsed[2],
               (_m6Owner != 0xFF) ? "yes" : "no");
    DBG_PRINTLN("[Mixer] ─────────────────────────────────────────────────");
}

// ── Outgoing messages ─────────────────────────────────────────────────────────

void MixerConnection::sendXRemote() {
    if (!_udpOpen) return;
    size_t len = OSCHandler::buildQuery("/xremote", _txBuf, sizeof(_txBuf));
    _udp.beginPacket(Config.mixerIP, Config.oscTxPort);
    _udp.write(_txBuf, len);
    _udp.endPacket();
}

// Poll fader/mute paths for all enabled non-meter triggers
void MixerConnection::sendFaderMuteQueries() {
    if (!_udpOpen) return;
    for (uint8_t n = 0; n < MAX_TRIGGERS; n++) {
        const TriggerConfig& t = Config.triggers[n];
        if (!t.enabled || t.signalSource == SIG_METER) continue;

        const String& path = _triggerPaths[n];
        if (path.length() == 0) continue;

        size_t len = OSCHandler::buildQuery(path, _txBuf, sizeof(_txBuf));
        _udp.beginPacket(Config.mixerIP, Config.oscTxPort);
        _udp.write(_txBuf, len);
        _udp.endPacket();
    }
}

// Register (or re-register) every meter subscription needed by the enabled
// SIG_METER triggers. Whole banks (/meters/0,1,2) are subscribed once each and
// every trigger picks its channel out of the shared blob by index — this avoids
// the /meters/6 collision (the console keeps only ONE global channel selection,
// so multiple /meters/6 subs would all report the same channel). The single
// permitted postfader trigger gets that one /meters/6 channel-strip sub.
// Subscriptions expire after ~10 s; renewMeterSubscriptions() keeps them alive.
void MixerConnection::sendMeterSubscriptions() {
    if (!_udpOpen) return;

    for (uint8_t b = 0; b < METER_BANK_COUNT; b++) {
        if (!_meterBankUsed[b]) continue;

        size_t len = OSCHandler::buildBatchSubscribe(
            kMeterAlias[b], kMeterPath[b], 0, kMeterIEnd[b], 1,
            _txBuf, sizeof(_txBuf));
        _udp.beginPacket(Config.mixerIP, Config.oscTxPort);
        _udp.write(_txBuf, len);
        _udp.endPacket();

        DBG_PRINTF("[Mixer] /batchsubscribe  %-4s %s 0 %d 1\n",
                   kMeterAlias[b], kMeterPath[b], (int)kMeterIEnd[b]);
    }

    if (_m6Owner != 0xFF && _m6ChannelId >= 0) {
        // /meters/6: first int = channel_id, second unused, then time factor.
        size_t len = OSCHandler::buildBatchSubscribe(
            kM6Alias, kM6Path, _m6ChannelId, 0, 1, _txBuf, sizeof(_txBuf));
        _udp.beginPacket(Config.mixerIP, Config.oscTxPort);
        _udp.write(_txBuf, len);
        _udp.endPacket();

        DBG_PRINTF("[Mixer] /batchsubscribe  %-4s %s ch=%d 0 1  (T%u post)\n",
                   kM6Alias, kM6Path, (int)_m6ChannelId, _m6Owner);
    }
}

// Lightweight renewal of ALL active subscriptions (one /renew with no argument
// resets the 10 s timer for every subscription registered by this client).
void MixerConnection::renewMeterSubscriptions() {
    if (!_udpOpen) return;
    size_t len = OSCHandler::buildQuery("/renew", _txBuf, sizeof(_txBuf));
    _udp.beginPacket(Config.mixerIP, Config.oscTxPort);
    _udp.write(_txBuf, len);
    _udp.endPacket();
}

// Stop ALL active subscriptions for this client (/unsubscribe with no argument).
// Sent before re-registering a changed meter set and when tearing down the link,
// so stale subscriptions (e.g. an old /meters/6 channel) stop immediately
// instead of lingering for up to 10 s.
void MixerConnection::unsubscribeAllMeters() {
    if (!_udpOpen) return;
    size_t len = OSCHandler::buildQuery("/unsubscribe", _txBuf, sizeof(_txBuf));
    _udp.beginPacket(Config.mixerIP, Config.oscTxPort);
    _udp.write(_txBuf, len);
    _udp.endPacket();
    _meterRegistered = false;
    DBG_PRINTLN("[Mixer] /unsubscribe (all)");
}

// ── Incoming messages ─────────────────────────────────────────────────────────

void MixerConnection::processIncoming() {
    IPAddress expectedIP;
    expectedIP.fromString(Config.mixerIP);

    int packetSize;
    while ((packetSize = _udp.parsePacket()) > 0) {
        IPAddress fromIP = _udp.remoteIP();
        if (fromIP != expectedIP) {
            if (millis() - _lastIgnoredPacketMs > 15000) {
                DBG_PRINTF("[Mixer] Ignoring UDP packet from %s\n", fromIP.toString().c_str());
                _lastIgnoredPacketMs = millis();
            }
            int discard = _udp.read(_rxBuf, sizeof(_rxBuf));
            (void)discard;
            continue;
        }

        int read = _udp.read(_rxBuf, sizeof(_rxBuf));
        if (read <= 0) continue;

        OSCMessage msg = OSCHandler::parse(_rxBuf, (size_t)read);
        if (!msg.valid) {
            if (millis() - _lastInvalidPacketMs > 15000) {
                DBG_PRINTF("[Mixer] Invalid OSC packet received from mixer %s\n",
                              fromIP.toString().c_str());
                _lastInvalidPacketMs = millis();
            }
            continue;
        }

        // Any valid OSC packet from the mixer keeps the connection alive —
        // including /info keepalive responses, xremote-pushed changes, etc.
        _lastResponseMs = millis();
        if (!_connected) {
            _connected = true;
            DBG_PRINTF("[Mixer] Connected from %s addr=%s type=%c\n",
                          fromIP.toString().c_str(), msg.address.c_str(), msg.typeTag);
        }

        // ── Meter blob (shared /meters/0,1,2 bank or the /meters/6 strip) ─────
        // One blob holds a whole bank; distribute it to every SIG_METER trigger
        // that reads from this bank, each at its own float index.
        if (msg.typeTag == 'b') {
            int bank = meterBankFromAlias(msg.address);
            if (bank >= 0) {
                _lastMeterRxMs = millis();   // subscriptions are alive (self-heal)
                for (uint8_t n = 0; n < MAX_TRIGGERS; n++) {
                    if (_meterBank[n] != (uint8_t)bank) continue;   // 0xFF != bank
                    float level = OSCHandler::extractMeterFloat(
                        _rxBuf, (size_t)read, msg.blobArgOffset, _meterIndex[n]);
                    _triggerLevels[n] = constrain(level, 0.0f, 1.0f);

                    // Throttled per-trigger debug: once per second
                    uint32_t nowDbg = millis();
                    if (nowDbg - _lastBlobDebugMs[n] >= 1000) {
                        _lastBlobDebugMs[n] = nowDbg;
                        DBG_PRINTF("[Mixer] T%u METER  /meters/%d idx=%-3u  "
                                   "floats=%d  level=%.4f\n",
                                   n, bank, (unsigned)_meterIndex[n],
                                   (int)msg.intVal, _triggerLevels[n]);
                    }
                }
                continue;   // meter blob handled; skip fader/mute matching
            }
        }

        // ── Fader / mute (one value per resolved path) ────────────────────────
        for (uint8_t n = 0; n < MAX_TRIGGERS; n++) {
            if (_triggerPaths[n].length() == 0) continue;   // meters use no path
            if (msg.address != _triggerPaths[n])  continue;

            float level = 0.0f;
            if (msg.typeTag == 'f') {
                level = msg.floatVal;
            } else if (msg.typeTag == 'i') {
                // Mute state: /mix/on → 1 = active, 0 = muted
                level = (float)msg.intVal;
            }
            _triggerLevels[n] = constrain(level, 0.0f, 1.0f);
        }
    }
}

// ── Main loop ─────────────────────────────────────────────────────────────────

void MixerConnection::loop() {
    uint32_t now = millis();

    if (Config.mixerIP[0] == '\0') return;

    // Every XREMOTE_INTERVAL_MS (< 10 s subscription timeout):
    //   • Renew /xremote so the X32 keeps pushing parameter changes
    //   • Keep meter subscriptions alive: a lightweight /renew normally, but
    //     re-register the full set if we never registered, or if no meter blob
    //     has arrived for >10 s (the console may have dropped a missed renew)
    //   • Send one-shot fader/mute queries to refresh current state
    //     (between renewals the X32 pushes changes via /xremote subscription)
    if (now - _lastXRemoteMs >= XREMOTE_INTERVAL_MS) {
        sendXRemote();

        const bool anyMeter = _meterBankUsed[0] || _meterBankUsed[1] ||
                              _meterBankUsed[2] || (_m6Owner != 0xFF);
        if (anyMeter) {
            if (_meterRegistered && (now - _lastMeterRxMs < 10000)) {
                renewMeterSubscriptions();      // light: /renew all
            } else {
                sendMeterSubscriptions();       // (re)register full set
                _meterRegistered = true;
            }
        }

        sendFaderMuteQueries();   // initial sync; X32 pushes changes between renewals
        _lastXRemoteMs = now;
    }

    // Lightweight keepalive: /info query every MIXER_KEEPALIVE_INTERVAL_MS.
    // Ensures the connection health indicator stays current even when the mixer
    // is completely idle (no fader movement, no meter subscriptions active).
    if (now - _lastKeepaliveMs >= MIXER_KEEPALIVE_INTERVAL_MS) {
        if (_udpOpen) {
            size_t len = OSCHandler::buildQuery("/info", _txBuf, sizeof(_txBuf));
            _udp.beginPacket(Config.mixerIP, Config.oscTxPort);
            _udp.write(_txBuf, len);
            _udp.endPacket();
        }
        _lastKeepaliveMs = now;
    }

    if (_udpOpen) processIncoming();

    // Re-read the current time after processing incoming packets.
    // processIncoming() may update _lastResponseMs to a later timestamp than the
    // loop-start time, so the timeout check must use a fresh clock value.
    now = millis();

    // Timeout
    if (_connected && (now - _lastResponseMs > MIXER_TIMEOUT_MS)) {
        _connected = false;
        DBG_PRINTF("[Mixer] Timeout — connection lost after %u ms. lastResponse=%u now=%u\n",
                      (unsigned)(now - _lastResponseMs), (unsigned)_lastResponseMs, (unsigned)now);
    }

    if (!_connected && now - _lastReconnectMs > MIXER_RECONNECT_INTERVAL_MS) {
        if (!_udpOpen) begin();
        _lastReconnectMs = now;
    }
}
