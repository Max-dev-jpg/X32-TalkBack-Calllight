// =============================================================================
// MixerConnection.cpp  –  OSC / UDP link to X32 / M32
// =============================================================================

#include "MixerConnection.h"
#include "ConfigManager.h"
#include "OSCHandler.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>

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
        sendMeterSubscriptions();
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
    if (_udpOpen) { _udp.stop(); _udpOpen = false; }
    _lastXRemoteMs  = 0;   // force immediate xremote + queries on next loop()
    _lastResponseMs = 0;
    _connected      = false;
    begin();
}

// ── Rebuild resolved path / alias strings ─────────────────────────────────────

void MixerConnection::rebuildPaths() {
    for (uint8_t n = 0; n < MAX_TRIGGERS; n++) {
        const TriggerConfig& t = Config.triggers[n];
        _meterChannelIds[n] = -1;
        if (!t.enabled) {
            _triggerPaths[n] = "";
            continue;
        }
        if (t.signalSource == SIG_METER) {
            // Responses arrive addressed to our alias (e.g. "/mt0")
            _triggerPaths[n]    = String("/mt") + n;
            _meterChannelIds[n] = ConfigManager::instance().meterChannelId(t);
        } else {
            _triggerPaths[n] = ConfigManager::instance().buildOSCPathForTrigger(t);
        }
    }
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

// Subscribe or renew /batchsubscribe for all enabled meter triggers
// Called every XREMOTE_INTERVAL_MS (same cadence as /xremote).
void MixerConnection::sendMeterSubscriptions() {
    if (!_udpOpen) return;
    for (uint8_t n = 0; n < MAX_TRIGGERS; n++) {
        const TriggerConfig& t = Config.triggers[n];
        if (!t.enabled || t.signalSource != SIG_METER) continue;

        int32_t chId = ConfigManager::instance().meterChannelId(t);
        if (chId < 0) continue; // DCA not available on /meters/6

        String alias = String("/mt") + n;
        // tf=2 → 100 Updates in the 10 s timeframe
        size_t len = OSCHandler::buildBatchSubscribe(alias, chId, 2,
                                                     _txBuf, sizeof(_txBuf));
        _udp.beginPacket(Config.mixerIP, Config.oscTxPort);
        _udp.write(_txBuf, len);
        _udp.endPacket();

        DBG_PRINTF("[Mixer] /batchsubscribe %s ch=%d\n",
                      alias.c_str(), (int)chId);
    }
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

        int read = _udp.read(_rxBuf, sizeof(_rxBuf) - 1);
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

        // Match the message address against each trigger's resolved path / alias
        for (uint8_t n = 0; n < MAX_TRIGGERS; n++) {
            if (_triggerPaths[n].length() == 0) continue;
            if (msg.address != _triggerPaths[n])  continue;

            float level = 0.0f;
            if (msg.typeTag == 'f') {
                level = msg.floatVal;
            } else if (msg.typeTag == 'i') {
                // Mute state: /mix/on → 1 = active, 0 = muted
                level = (float)msg.intVal;
            } else if (msg.typeTag == 'b') {
                // Blob from /batchsubscribe — the X32 sends the full meter bank;
                // read the float at the channel index we subscribed to.
                uint32_t chIdx = (_meterChannelIds[n] >= 0) ? (uint32_t)_meterChannelIds[n] : 0;
                level = OSCHandler::extractMeterFloat(_rxBuf, (size_t)read,
                                                       msg.blobArgOffset, chIdx);
            }

            level = constrain(level, 0.0f, 1.0f);
            _triggerLevels[n] = level;
        }
    }
}

// ── Main loop ─────────────────────────────────────────────────────────────────

void MixerConnection::loop() {
    uint32_t now = millis();

    if (Config.mixerIP[0] == '\0') return;

    // Every XREMOTE_INTERVAL_MS:
    //   • Renew /xremote so the X32 keeps pushing parameter changes for 10 s
    //   • Renew /batchsubscribe for any meter-mode triggers
    //   • Send one-shot fader/mute queries to refresh current state
    //     (between renewals the X32 pushes changes via /xremote subscription)
    if (now - _lastXRemoteMs >= XREMOTE_INTERVAL_MS) {
        sendXRemote();
        sendMeterSubscriptions();
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
