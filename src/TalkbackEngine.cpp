// =============================================================================
// TalkbackEngine.cpp
// =============================================================================

#include "TalkbackEngine.h"
#include "ConfigManager.h"
#include "OSCHandler.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>

// ─────────────────────────────────────────────────────────────────────────────

void TalkbackEngine::begin() {
    if (!Config.tbEnabled) return;

    if (_udp.begin(TB_RX_PORT)) {
        _udpOpen = true;
        Serial.printf("[TB] UDP listening on port %d\n", TB_RX_PORT);
    } else {
        Serial.println("[TB] UDP begin failed!");
    }
}

// ── UDP helpers ───────────────────────────────────────────────────────────────

void TalkbackEngine::sendQuery(const String& address) {
    if (!_udpOpen) return;
    size_t len = OSCHandler::buildQuery(address, _txBuf, sizeof(_txBuf));
    _udp.beginPacket(Config.mixerIP, Config.oscTxPort);
    _udp.write(_txBuf, len);
    _udp.endPacket();
}

void TalkbackEngine::sendInt(const String& address, int32_t value) {
    if (!_udpOpen) return;
    size_t len = OSCHandler::buildIntMsg(address, value, _txBuf, sizeof(_txBuf));
    _udp.beginPacket(Config.mixerIP, Config.oscTxPort);
    _udp.write(_txBuf, len);
    _udp.endPacket();
}

void TalkbackEngine::sendNoArg(const String& address) {
    sendQuery(address);   // query with empty type tag == no-arg message
}

// ── Solo ID ───────────────────────────────────────────────────────────────────

uint8_t TalkbackEngine::soloID() const {
    uint8_t n = Config.tbSoloNumber;
    switch (Config.tbSoloType) {
        case CH_INPUT:  return SOLO_OFFSET_INPUT  + n;   // 1-32
        case CH_AUXIN:  return SOLO_OFFSET_AUXIN  + n;   // 33-40
        case CH_BUS:    return SOLO_OFFSET_BUS    + n;   // 49-64
        case CH_MATRIX: return SOLO_OFFSET_MATRIX + n;   // 65-70
        case CH_DCA:    return SOLO_OFFSET_DCA    + n;   // 73-80
        default:        return n;
    }
}

// ── State-change handlers ─────────────────────────────────────────────────────

void TalkbackEngine::onTalkbackOn() {
    Serial.println("[TB] TALKBACK ON");

    // 1. Clear all solos first (if configured)
    if (Config.tbClearSolo) {
        sendNoArg(TB_CLEARSOLO_PATH);
        Serial.println("[TB]   -> clear solo");
    }

    // 2. Send solo ON for configured channel
    if (Config.tbSoloEnabled) {
        String path = String(TB_SOLOSW_BASE) + soloID();
        sendInt(path, 1);
        Serial.printf("[TB]   -> solo ON  path=%s  id=%d\n",
                      path.c_str(), soloID());
    }

    // 3. Custom OSC commands
    if (Config.tbOnCmd1[0] != '\0') {
        sendInt(String(Config.tbOnCmd1), 1);
        Serial.printf("[TB]   -> custom ON cmd1: %s\n", Config.tbOnCmd1);
    }
    if (Config.tbOnCmd2[0] != '\0') {
        sendInt(String(Config.tbOnCmd2), 1);
        Serial.printf("[TB]   -> custom ON cmd2: %s\n", Config.tbOnCmd2);
    }
}

void TalkbackEngine::onTalkbackOff() {
    Serial.println("[TB] TALKBACK OFF");

    // 1. Un-solo the channel
    if (Config.tbSoloEnabled) {
        String path = String(TB_SOLOSW_BASE) + soloID();
        sendInt(path, 0);
        Serial.printf("[TB]   -> solo OFF  path=%s  id=%d\n",
                      path.c_str(), soloID());
    }

    // 2. Custom OSC commands (sent with int=0)
    if (Config.tbOffCmd1[0] != '\0') {
        sendInt(String(Config.tbOffCmd1), 0);
        Serial.printf("[TB]   -> custom OFF cmd1: %s\n", Config.tbOffCmd1);
    }
    if (Config.tbOffCmd2[0] != '\0') {
        sendInt(String(Config.tbOffCmd2), 0);
        Serial.printf("[TB]   -> custom OFF cmd2: %s\n", Config.tbOffCmd2);
    }
}

// ── Incoming UDP parser ───────────────────────────────────────────────────────

void TalkbackEngine::processIncoming() {
    int sz = _udp.parsePacket();
    if (sz <= 0) return;

    int rd = _udp.read(_rxBuf, sizeof(_rxBuf) - 1);
    if (rd <= 0) return;

    OSCMessage msg = OSCHandler::parse(_rxBuf, (size_t)rd);
    if (!msg.valid) return;

    // Talkback A state
    if (msg.address == TB_PATH_A &&
        (Config.tbMonitor == TB_MONITOR_A || Config.tbMonitor == TB_MONITOR_BOTH)) {
        bool active = (msg.typeTag == 'i') ? (msg.intVal != 0)
                                           : (msg.floatVal >= 0.5f);
        if (active != _stateA) {
            _stateA = active;
            if (_stateA) onTalkbackOn(); else onTalkbackOff();
        }
    }

    // Talkback B state
    if (msg.address == TB_PATH_B &&
        (Config.tbMonitor == TB_MONITOR_B || Config.tbMonitor == TB_MONITOR_BOTH)) {
        bool active = (msg.typeTag == 'i') ? (msg.intVal != 0)
                                           : (msg.floatVal >= 0.5f);
        if (active != _stateB) {
            _stateB = active;
            if (_stateB) onTalkbackOn(); else onTalkbackOff();
        }
    }
}

// ── Main loop ─────────────────────────────────────────────────────────────────

void TalkbackEngine::loop() {
    if (!Config.tbEnabled || !_udpOpen) return;

    uint32_t now = millis();

    // Renew /xremote so the X32 pushes changes to us
    if (now - _lastXRemoteMs >= XREMOTE_INTERVAL_MS) {
        sendNoArg("/xremote");
        _lastXRemoteMs = now;
    }

    // Poll talkback state(s)
    if (now - _lastPollMs >= TB_POLL_INTERVAL_MS) {
        if (Config.tbMonitor == TB_MONITOR_A || Config.tbMonitor == TB_MONITOR_BOTH)
            sendQuery(TB_PATH_A);
        if (Config.tbMonitor == TB_MONITOR_B || Config.tbMonitor == TB_MONITOR_BOTH)
            sendQuery(TB_PATH_B);
        _lastPollMs = now;
    }

    // Read responses
    processIncoming();
}
