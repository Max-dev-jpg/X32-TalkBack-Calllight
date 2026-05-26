// =============================================================================
// TalkbackEngine.cpp  –  Monitors X32/M32 talkback buttons; delegates actions
// =============================================================================

#include "TalkbackEngine.h"
#include "ActionEngine.h"
#include "ConfigManager.h"
#include "OSCHandler.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>

// ─────────────────────────────────────────────────────────────────────────────

void TalkbackEngine::begin() {
    _stateA = false;
    _stateB = false;

    // Clear any output overrides that TB may have left
    ActionEngine::clearOutput(ACT_SRC_TB_A);
    ActionEngine::clearOutput(ACT_SRC_TB_B);

    if (!Config.tbEnabled) return;

    // Re-open UDP (stop first so begin() is idempotent on config changes)
    _udp.stop();
    _udpOpen = false;

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
    sendQuery(address);
}

// ── State-change handlers ─────────────────────────────────────────────────────

void TalkbackEngine::onTalkbackOn(bool isA) {
    Serial.printf("[TB] TALKBACK %s ON\n", isA ? "A" : "B");
    ActionEngine::execute(isA ? Config.tbAOnJson : Config.tbBOnJson,
                          isA ? ACT_SRC_TB_A    : ACT_SRC_TB_B);
}

void TalkbackEngine::onTalkbackOff(bool isA) {
    Serial.printf("[TB] TALKBACK %s OFF\n", isA ? "A" : "B");
    ActionEngine::execute(isA ? Config.tbAOffJson : Config.tbBOffJson,
                          isA ? ACT_SRC_TB_A      : ACT_SRC_TB_B);
    // Clear this button's output override regardless of what the action list says
    ActionEngine::clearOutput(isA ? ACT_SRC_TB_A : ACT_SRC_TB_B);
}

// ── External simulation (from OSC receiver) ───────────────────────────────────

void TalkbackEngine::simulateTalkback(bool isA, bool active) {
    bool& state = isA ? _stateA : _stateB;
    if (active != state) {
        state = active;
        if (active) onTalkbackOn(isA);
        else        onTalkbackOff(isA);
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

    // Talkback A
    if (msg.address == TB_PATH_A &&
        (Config.tbMonitor == TB_MONITOR_A || Config.tbMonitor == TB_MONITOR_BOTH)) {
        bool active = (msg.typeTag == 'i') ? (msg.intVal != 0)
                                           : (msg.floatVal >= 0.5f);
        if (active != _stateA) {
            _stateA = active;
            if (_stateA) onTalkbackOn(true); else onTalkbackOff(true);
        }
    }

    // Talkback B
    if (msg.address == TB_PATH_B &&
        (Config.tbMonitor == TB_MONITOR_B || Config.tbMonitor == TB_MONITOR_BOTH)) {
        bool active = (msg.typeTag == 'i') ? (msg.intVal != 0)
                                           : (msg.floatVal >= 0.5f);
        if (active != _stateB) {
            _stateB = active;
            if (_stateB) onTalkbackOn(false); else onTalkbackOff(false);
        }
    }
}

// ── Main loop ─────────────────────────────────────────────────────────────────

void TalkbackEngine::loop() {
    if (!Config.tbEnabled || !_udpOpen) return;

    uint32_t now = millis();

    // Renew /xremote so the X32/M32 pushes changes to us
    if (now - _lastXRemoteMs >= XREMOTE_INTERVAL_MS) {
        sendNoArg("/xremote");
        _lastXRemoteMs = now;
    }

    // Poll talkback state
    if (now - _lastPollMs >= TB_POLL_INTERVAL_MS) {
        if (Config.tbMonitor == TB_MONITOR_A || Config.tbMonitor == TB_MONITOR_BOTH)
            sendQuery(TB_PATH_A);
        if (Config.tbMonitor == TB_MONITOR_B || Config.tbMonitor == TB_MONITOR_BOTH)
            sendQuery(TB_PATH_B);
        _lastPollMs = now;
    }

    processIncoming();
}
