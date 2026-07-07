// =============================================================================
// MixerConnection.cpp  –  OSC / UDP link to X32 / M32
// =============================================================================

#include "MixerConnection.h"
#include "ConfigManager.h"
#include "OSCHandler.h"
#include "MeterScale.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>

// ── Shared meter-bank subscriptions ────────────────────────────────────────────
// One full-bank subscription per used bank; the console returns the blob with the
// alias as its OSC address. iEnd is the INCLUSIVE last float index of the bank.
namespace {
    // Every meter source is a /batchsubscribe; the console returns each blob
    // addressed to our alias. /m0,/m1,/m2 are the full banks (iEnd = inclusive
    // last float index); /m6 is the single-channel strip of /meters/6, whose
    // first int selects the channel. All are renewed together by one /renew.
    const char* const kMeterAlias[3] = { "/m0", "/m1", "/m2" };
    const char* const kMeterPath [3] = { "/meters/0", "/meters/1", "/meters/2" };
    const int32_t     kMeterIEnd [3] = { 69, 95, 48 };  // 70 / 96 / 49 floats
    const char* const kM6Alias       = "/m6";

    // Map an alias to its bank id: 0..2 for the full banks, 3 (METER_BANK_M6) for
    // the /meters/6 strip, or -1 if it is not one of our aliases.
    int meterBankFromAlias(const String& addr) {
        for (int b = 0; b < 3; b++)
            if (addr == kMeterAlias[b]) return b;
        if (addr == kM6Alias) return 3;
        return -1;
    }
}

// ─────────────────────────────────────────────────────────────────────────────

// Open the OSC receive socket and build the initial trigger path/meter routing.
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
        _metersRegistered = true;
        _lastMeterRxMs    = now;
        _lastMeterRenewMs = now;
        _lastXRemoteMs    = now;
        _lastKeepaliveMs  = now;
        _lastReconnectMs  = now;
        _lastResponseMs   = now;
    }
}

// Drop meter subscriptions and reopen the socket (e.g. after a config change).
void MixerConnection::reconnect() {
    DBG_PRINTLN("[Mixer] Reconnecting...");
    if (_udpOpen) {
        unsubscribeAllMeters();      // drop stale subs (e.g. old /meters/6 channel)
        _udp.stop();
        _udpOpen = false;
    }
    _lastXRemoteMs    = 0;   // force immediate xremote + queries on next loop()
    _lastResponseMs   = 0;
    _connected        = false;
    _metersRegistered = false;
    begin();
}

// ── Rebuild resolved path / alias strings ─────────────────────────────────────

void MixerConnection::rebuildPaths() {
    for (uint8_t b = 0; b < METER_BANK_COUNT; b++) _meterBankUsed[b] = false;
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
        // Start every trigger at its polarity-aware at-rest level, so before the
        // first real value arrives it reads INACTIVE (a fader/mute trigger would
        // otherwise read a stale 0 and an inverted one would fire on boot).
        _triggerLevels[n] = MeterScale::restLevel(t.signalSource, t.meterSignalType, t.invert,
                                                  t.customOSCPath[0] != '\0');
        if (t.signalSource == SIG_METER) {
            // Meters use no _triggerPaths entry; the fader/mute matcher ignores
            // them. meterRoute() picks the bank + index. /meters/6 has ONE
            // channel selection: the first such trigger claims a channel; further
            // /meters/6 triggers on a DIFFERENT channel are blocked.
            _triggerPaths[n] = "";
            uint8_t bank; uint32_t idx;
            if (ConfigManager::instance().meterRoute(t, bank, idx)) {
                if (bank == ConfigManager::METER_ROUTE_M6) {
                    int32_t ch = ConfigManager::instance().meterChannelId(t);
                    if (ch < 0) {
                        // no meter (shouldn't happen for /m6 types)
                    } else if (_m6ChannelId == -1 || _m6ChannelId == (int16_t)ch) {
                        _m6ChannelId   = (int16_t)ch;   // claim (or share) the channel
                        _meterBank[n]  = METER_BANK_M6;
                        _meterIndex[n] = idx;           // tap within the 4-float strip
                    } else {
                        _meterBlocked[n] = true;        // different channel — blocked
                    }
                } else {
                    _meterBank[n]  = bank;              // full bank 0/1/2
                    _meterIndex[n] = idx;
                    _meterBankUsed[bank] = true;
                }
            }

            // Blocked or unsupported (e.g. DCA) meters keep the at-rest level set
            // above, so a stale value can't hold the trigger active.
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
                DBG_PRINTF("[Mixer]   T%u: %-6s  ch=%-6s #%-2u  /meters/6 ch=%d tap=%u\n",
                           n, sigStr, chStr, t.channelNumber,
                           (int)_m6ChannelId, (unsigned)_meterIndex[n]);
            } else if (_meterBlocked[n]) {
                DBG_PRINTF("[Mixer]   T%u: %-6s  ch=%-6s #%-2u  BLOCKED (/meters/6 busy on ch %d)\n",
                           n, sigStr, chStr, t.channelNumber, (int)_m6ChannelId);
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
    DBG_PRINTF("[Mixer]   meter banks: /meters/0=%d /meters/1=%d /meters/2=%d  "
               "/meters/6 channel=%d\n",
               _meterBankUsed[0], _meterBankUsed[1], _meterBankUsed[2], (int)_m6ChannelId);
    DBG_PRINTLN("[Mixer] ─────────────────────────────────────────────────");
}

// ── Outgoing messages ─────────────────────────────────────────────────────────

// Send /xremote so the console pushes real-time parameter changes to us.
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

// (Re)register every meter source via /batchsubscribe: the full banks
// /meters/0,1,2 and, if a post-fader/Main-pre trigger needs it, the single
// /meters/6 channel (first int = channel_id). Called on connect/config-change
// and by the self-heal; steady-state extension is done with renewMeterSubscriptions().
void MixerConnection::sendMeterSubscriptions() {
    if (!_udpOpen) return;

    for (uint8_t b = 0; b < METER_BANK_COUNT; b++) {
        if (!_meterBankUsed[b]) continue;

        size_t len = OSCHandler::buildBatchSubscribe(
            kMeterAlias[b], kMeterPath[b], 0, kMeterIEnd[b], METER_SUBSCRIBE_TF,
            _txBuf, sizeof(_txBuf));
        _udp.beginPacket(Config.mixerIP, Config.oscTxPort);
        _udp.write(_txBuf, len);
        _udp.endPacket();

        DBG_PRINTF("[Mixer] /batchsubscribe  %-4s %s 0 %d 1\n",
                   kMeterAlias[b], kMeterPath[b], (int)kMeterIEnd[b]);
    }

    if (_m6ChannelId >= 0) {
        // /meters/6: first int = channel_id, second unused, then time factor.
        size_t len = OSCHandler::buildBatchSubscribe(
            kM6Alias, "/meters/6", _m6ChannelId, 0, METER_SUBSCRIBE_TF, _txBuf, sizeof(_txBuf));
        _udp.beginPacket(Config.mixerIP, Config.oscTxPort);
        _udp.write(_txBuf, len);
        _udp.endPacket();
        DBG_PRINTF("[Mixer] /batchsubscribe  %-4s /meters/6 ch=%d\n",
                   kM6Alias, (int)_m6ChannelId);
    }
}

// Extend ALL active subscriptions with one no-arg /renew (resets their 10 s timer
// without re-sending the parameters). They were all registered together, so they
// renew together. Cheaper than re-subscribing each cycle.
void MixerConnection::renewMeterSubscriptions() {
    if (!_udpOpen) return;
    size_t len = OSCHandler::buildQuery("/renew", _txBuf, sizeof(_txBuf));
    _udp.beginPacket(Config.mixerIP, Config.oscTxPort);
    _udp.write(_txBuf, len);
    _udp.endPacket();
    DBG_PRINTLN("[Mixer] /renew (all subscriptions)");
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
    DBG_PRINTLN("[Mixer] /unsubscribe (all)");
}

// ── Incoming messages ─────────────────────────────────────────────────────────

// Drain queued UDP packets: parse meter blobs and fader/mute replies, convert to
// dB, and update the per-trigger levels + connection liveness.
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

        // ── Meter blob (/m0,/m1,/m2 full bank, or /m6 single-channel strip) ───
        // Distribute to every SIG_METER trigger reading that bank, each at its own
        // float index. For /m6 (bank 3) the index is the tap within the 4-float
        // strip; all /m6 triggers share the one selected channel.
        if (msg.typeTag == 'b') {
            int bank = meterBankFromAlias(msg.address);
            if (bank >= 0) {
                _lastMeterRxMs = millis();   // subscriptions are alive
                for (uint8_t n = 0; n < MAX_TRIGGERS; n++) {
                    if (_meterBank[n] != (uint8_t)bank) continue;   // 0xFF != bank
                    float raw = OSCHandler::extractMeterFloat(
                        _rxBuf, (size_t)read, msg.blobArgOffset, _meterIndex[n]);
                    // Convert to dB here so the trigger filters + UI work in dB.
                    _triggerLevels[n] = MeterScale::toDb(
                        SIG_METER, Config.triggers[n].meterSignalType, raw);

                    // Throttled per-trigger debug: once per second
                    uint32_t nowDbg = millis();
                    if (nowDbg - _lastBlobDebugMs[n] >= 1000) {
                        _lastBlobDebugMs[n] = nowDbg;
                        DBG_PRINTF("[Mixer] T%u METER  %-3s idx=%-3u  raw=%.4f  %.1f dB\n",
                                   n, msg.address.c_str(), (unsigned)_meterIndex[n],
                                   raw, _triggerLevels[n]);
                    }
                }
                continue;   // meter blob handled; skip fader/mute matching
            }
        }

        // ── Fader / mute (one value per resolved path) ────────────────────────
        for (uint8_t n = 0; n < MAX_TRIGGERS; n++) {
            if (_triggerPaths[n].length() == 0) continue;   // meters use no path
            if (msg.address != _triggerPaths[n])  continue;

            float raw = 0.0f;
            if (msg.typeTag == 'f') {
                raw = msg.floatVal;                      // fader position 0..1
            } else if (msg.typeTag == 'i') {
                raw = (float)msg.intVal;                 // /mix/on: 1=active, 0=muted
            }
            // Fader → dB (X32 taper); mute passes through 0/1. A custom OSC path
            // (pan, EQ freq, …) is used linearly in a 0..1 domain instead.
            bool custom = Config.triggers[n].customOSCPath[0] != '\0';
            _triggerLevels[n] = MeterScale::toDb(
                Config.triggers[n].signalSource, 0, constrain(raw, 0.0f, 1.0f), custom);
        }
    }
}

// ── Main loop ─────────────────────────────────────────────────────────────────

// Service the link each iteration: process incoming OSC, renew /xremote and meter
// subscriptions on schedule, self-heal stalled meters, and track connection state.
void MixerConnection::loop() {
    uint32_t now = millis();

    if (Config.mixerIP[0] == '\0') return;

    // Every XREMOTE_INTERVAL_MS: renew /xremote so the X32 keeps pushing parameter
    // changes, and send one-shot fader/mute queries to refresh current state
    // (between renewals the X32 pushes changes via the /xremote subscription).
    if (now - _lastXRemoteMs >= XREMOTE_INTERVAL_MS) {
        sendXRemote();
        sendFaderMuteQueries();
        _lastXRemoteMs = now;
    }

    // Every METER_RENEW_INTERVAL_MS: extend the meter subscriptions with a
    // lightweight /renew if registered, else (re)register them in full. All meter
    // subs renew together, so a lost /renew lapses them all at once — caught by
    // the self-heal below rather than leaving one bank silently dead.
    if (now - _lastMeterRenewMs >= METER_RENEW_INTERVAL_MS) {
        if (_metersRegistered) {
            renewMeterSubscriptions();
        } else {
            sendMeterSubscriptions();
            _metersRegistered = true;
            _lastMeterRxMs    = now;
        }
        _lastMeterRenewMs = now;
    }

    // Self-heal: if we expect meter data (any bank or /meters/6 in use) but none
    // has arrived for METER_STALL_MS while connected, the subs lapsed (e.g. a lost
    // /renew). Re-register them.
    const bool anyMeter = _meterBankUsed[0] || _meterBankUsed[1] ||
                          _meterBankUsed[2] || (_m6ChannelId >= 0);
    if (_udpOpen && _connected && _metersRegistered && anyMeter &&
        (now - _lastMeterRxMs > METER_STALL_MS)) {
        DBG_PRINTLN("[Mixer] meter data stalled — re-registering subscriptions");
        sendMeterSubscriptions();
        _lastMeterRxMs = now;   // give the resubscribe time before retrying
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
