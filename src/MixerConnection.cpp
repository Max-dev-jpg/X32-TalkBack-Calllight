// =============================================================================
// MixerConnection.cpp  –  OSC / UDP link to X32 / M32
// =============================================================================

#include "MixerConnection.h"
#include "ConfigManager.h"
#include "OSCHandler.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>

void MixerConnection::begin() {
    Serial.println("[Mixer] Initialising UDP...");
    if (_udp.begin(Config.oscRxPort)) {
        _udpOpen = true;
        Serial.printf("[Mixer] Listening on UDP port %d\n", Config.oscRxPort);
    } else {
        Serial.println("[Mixer] UDP begin FAILED!");
    }
}

void MixerConnection::reconnect() {
    Serial.println("[Mixer] Reconnecting...");
    if (_udpOpen) {
        _udp.stop();
        _udpOpen = false;
    }
    begin();
    _lastXRemoteMs  = 0;  // force immediate /xremote send
    _lastPollMs     = 0;
    _lastResponseMs = 0;
    _connected      = false;
}

// ─────────────────────────────────────────────────────────────────────────────

void MixerConnection::sendXRemote() {
    if (!_udpOpen) return;
    size_t len = OSCHandler::buildQuery("/xremote", _txBuf, sizeof(_txBuf));
    _udp.beginPacket(Config.mixerIP, Config.oscTxPort);
    _udp.write(_txBuf, len);
    _udp.endPacket();
}

void MixerConnection::sendQuery() {
    if (!_udpOpen) return;
    String path = ConfigManager::instance().buildOSCPath();

    size_t len;
    if (Config.signalSource == SIG_METER) {
        // Subscribe to meter bus
        len = OSCHandler::buildStringMsg("/meters", path, _txBuf, sizeof(_txBuf));
    } else {
        len = OSCHandler::buildQuery(path, _txBuf, sizeof(_txBuf));
    }

    _udp.beginPacket(Config.mixerIP, Config.oscTxPort);
    _udp.write(_txBuf, len);
    _udp.endPacket();
}

void MixerConnection::processIncoming() {
    int packetSize = _udp.parsePacket();
    if (packetSize <= 0) return;

    int read = _udp.read(_rxBuf, sizeof(_rxBuf) - 1);
    if (read <= 0) return;

    OSCMessage msg = OSCHandler::parse(_rxBuf, (size_t)read);
    if (!msg.valid) return;

    String expectedPath = ConfigManager::instance().buildOSCPath();

    // Accept response if address matches our query path
    if (msg.address == expectedPath || msg.address.startsWith("/meters")) {
        float level = 0.0f;

        if (msg.typeTag == 'f') {
            level = msg.floatVal;
        } else if (msg.typeTag == 'i') {
            // Mute state: /mix/on  —  1 = active, 0 = muted
            level = (float)msg.intVal;
        } else if (msg.typeTag == 'b') {
            // Meter blob: each channel is an int16 scaled 0..32767
            // Parse channel index from the path (simplified: use ch 0)
            if (read >= 12 + 4 + 2) {  // rough guard
                // Skip to first int16 after OSC headers (best-effort)
                // A proper implementation would parse the blob length first
                uint8_t* blob = _rxBuf + (read - 2);
                int16_t raw = (int16_t)((blob[0] << 8) | blob[1]);
                level = (float)raw / 32767.0f;
            }
        }

        // Clamp to 0-1
        level = constrain(level, 0.0f, 1.0f);
        _currentLevel  = level;
        _lastResponseMs = millis();

        if (!_connected) {
            _connected = true;
            Serial.printf("[Mixer] Connected  level=%.3f\n", level);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────

void MixerConnection::loop() {
    uint32_t now = millis();

    // Only operate when WiFi is available (AP-only is fine; we check IP)
    if (Config.mixerIP[0] == '\0') return;

    // Renew /xremote subscription
    if (now - _lastXRemoteMs >= XREMOTE_INTERVAL_MS) {
        sendXRemote();
        _lastXRemoteMs = now;
    }

    // Poll configured path
    if (now - _lastPollMs >= OSC_POLL_INTERVAL_MS) {
        sendQuery();
        _lastPollMs = now;
    }

    // Read any incoming UDP packets
    if (_udpOpen) {
        processIncoming();
    }

    // Timeout detection
    if (_connected && (now - _lastResponseMs > MIXER_TIMEOUT_MS)) {
        _connected = false;
        Serial.println("[Mixer] Timeout — connection lost.");
    }

    // Attempt reconnect if not connected
    if (!_connected && now - _lastReconnectMs > MIXER_RECONNECT_INTERVAL_MS) {
        if (!_udpOpen) begin();
        _lastReconnectMs = now;
    }
}
