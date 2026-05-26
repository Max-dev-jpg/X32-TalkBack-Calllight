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
    Serial.println("[Mixer] Initialising UDP...");
    if (_udpOpen) {
        _udp.stop();
        _udpOpen = false;
    }
    if (_udp.begin(Config.oscRxPort)) {
        _udpOpen = true;
        Serial.printf("[Mixer] Listening on UDP port %d\n", Config.oscRxPort);
    } else {
        Serial.println("[Mixer] UDP begin FAILED!");
    }
    rebuildPaths();
}

void MixerConnection::reconnect() {
    Serial.println("[Mixer] Reconnecting...");
    if (_udpOpen) { _udp.stop(); _udpOpen = false; }
    _lastXRemoteMs  = 0;
    _lastPollMs     = 0;
    _lastResponseMs = 0;
    _connected      = false;
    begin();
}

// ── Rebuild resolved path / alias strings ─────────────────────────────────────

void MixerConnection::rebuildPaths() {
    for (uint8_t n = 0; n < MAX_TRIGGERS; n++) {
        const TriggerConfig& t = Config.triggers[n];
        if (!t.enabled) {
            _triggerPaths[n] = "";
            continue;
        }
        if (t.signalSource == SIG_METER) {
            // Responses arrive addressed to our alias (e.g. "/mt0")
            _triggerPaths[n] = String("/mt") + n;
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
        // tf=10 → 10 frames × 5 ms = 50 ms per update; subscription lives 10 s
        size_t len = OSCHandler::buildBatchSubscribe(alias, chId, 10,
                                                     _txBuf, sizeof(_txBuf));
        _udp.beginPacket(Config.mixerIP, Config.oscTxPort);
        _udp.write(_txBuf, len);
        _udp.endPacket();

        Serial.printf("[Mixer] /batchsubscribe %s ch=%d\n",
                      alias.c_str(), (int)chId);
    }
}

// ── Incoming messages ─────────────────────────────────────────────────────────

void MixerConnection::processIncoming() {
    int packetSize = _udp.parsePacket();
    if (packetSize <= 0) return;

    int read = _udp.read(_rxBuf, sizeof(_rxBuf) - 1);
    if (read <= 0) return;

    OSCMessage msg = OSCHandler::parse(_rxBuf, (size_t)read);
    if (!msg.valid) return;

    // Match the message address against each trigger's resolved path / alias
    bool matched = false;
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
            // Blob from /batchsubscribe: floatVal holds the first LE float
            level = msg.floatVal;
        }

        level = constrain(level, 0.0f, 1.0f);
        _triggerLevels[n] = level;
        matched = true;
    }

    if (matched) {
        _lastResponseMs = millis();
        if (!_connected) {
            _connected = true;
            Serial.println("[Mixer] Connected.");
        }
    }
}

// ── Main loop ─────────────────────────────────────────────────────────────────

void MixerConnection::loop() {
    uint32_t now = millis();

    if (Config.mixerIP[0] == '\0') return;

    // Renew /xremote (covers fader/mute subscriptions) and meter subscriptions
    if (now - _lastXRemoteMs >= XREMOTE_INTERVAL_MS) {
        sendXRemote();
        sendMeterSubscriptions();
        _lastXRemoteMs = now;
    }

    // Poll fader/mute paths
    if (now - _lastPollMs >= OSC_POLL_INTERVAL_MS) {
        sendFaderMuteQueries();
        _lastPollMs = now;
    }

    if (_udpOpen) processIncoming();

    // Timeout
    if (_connected && (now - _lastResponseMs > MIXER_TIMEOUT_MS)) {
        _connected = false;
        Serial.println("[Mixer] Timeout — connection lost.");
    }

    if (!_connected && now - _lastReconnectMs > MIXER_RECONNECT_INTERVAL_MS) {
        if (!_udpOpen) begin();
        _lastReconnectMs = now;
    }
}
