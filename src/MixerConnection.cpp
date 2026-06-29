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
    // Full banks are subscribed with /batchsubscribe; the console returns each
    // bank's blob addressed to our alias. /meters/6 is NOT batch-subscribed — the
    // scanner selects it with the direct "/meters ,si /meters/6 <ch>" form, whose
    // reply is addressed to "/meters/6" (handled separately in processIncoming).
    const char* const kMeterAlias[3] = { "/m0", "/m1", "/m2" };
    const char* const kMeterPath [3] = { "/meters/0", "/meters/1", "/meters/2" };
    const int32_t     kMeterIEnd [3] = { 69, 95, 48 };  // 70 / 96 / 49 floats

    // Map a full-bank alias to its bank index 0..2, or -1 if not one of ours.
    int meterBankFromAlias(const String& addr) {
        for (int b = 0; b < 3; b++)
            if (addr == kMeterAlias[b]) return b;
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

            // No live meter source (blocked or unsupported, e.g. DCA): level 0.
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

// Register (or re-register) the meter sources needed by the enabled SIG_METER
// triggers, and renew them (called on the < 10 s /xremote cadence):
//   • Full banks /meters/0,1,2 via /batchsubscribe — re-sending resets the 10 s
//     timer, so this both registers and renews them.
//   • The single /meters/6 channel (post-fader / Main pre) via the direct
//     "/meters ,si /meters/6 <ch>" form. Only one channel can be selected
//     console-wide, so exactly one channel is requested here.
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

    if (_m6ChannelId >= 0) {
        size_t len = OSCHandler::buildMetersSelect("/meters/6", _m6ChannelId,
                                                   _txBuf, sizeof(_txBuf));
        _udp.beginPacket(Config.mixerIP, Config.oscTxPort);
        _udp.write(_txBuf, len);
        _udp.endPacket();
        DBG_PRINTF("[Mixer] /meters ,si /meters/6 %d\n", (int)_m6ChannelId);
    }
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

        // ── /meters/6 channel strip (direct-form reply, addr "/meters/6") ─────
        // Holds the single selected channel; distribute to every trigger reading
        // /meters/6 (all share the one selected channel) at its own tap index.
        if (msg.typeTag == 'b' && msg.address == "/meters/6") {
            for (uint8_t n = 0; n < MAX_TRIGGERS; n++) {
                if (_meterBank[n] != METER_BANK_M6) continue;
                float level = OSCHandler::extractMeterFloat(
                    _rxBuf, (size_t)read, msg.blobArgOffset, _meterIndex[n]);
                _triggerLevels[n] = constrain(level, 0.0f, 1.0f);

                uint32_t nowDbg = millis();
                if (nowDbg - _lastBlobDebugMs[n] >= 1000) {
                    _lastBlobDebugMs[n] = nowDbg;
                    DBG_PRINTF("[Mixer] T%u METER  /meters/6 ch=%d tap=%u  level=%.4f\n",
                               n, (int)_m6ChannelId, (unsigned)_meterIndex[n],
                               _triggerLevels[n]);
                }
            }
            continue;
        }

        // ── Full-bank meter blob (/meters/0,1,2) ──────────────────────────────
        // One blob holds a whole bank; distribute it to every SIG_METER trigger
        // that reads from this bank, each at its own float index.
        if (msg.typeTag == 'b') {
            int bank = meterBankFromAlias(msg.address);
            if (bank >= 0) {
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
    //   • Re-send the full meter /batchsubscribe set. Re-sending a batchsubscribe
    //     also resets its 10 s timer, so this both registers and renews — and it
    //     renews EVERY bank reliably (a no-arg /renew proved fragile: it could let
    //     one bank, e.g. /meters/6, lapse while others kept the link alive). The
    //     traffic is a handful of small packets, so cost is negligible.
    //   • Send one-shot fader/mute queries to refresh current state
    //     (between renewals the X32 pushes changes via /xremote subscription)
    if (now - _lastXRemoteMs >= XREMOTE_INTERVAL_MS) {
        sendXRemote();
        sendMeterSubscriptions();
        sendFaderMuteQueries();
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
