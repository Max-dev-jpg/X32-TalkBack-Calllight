// =============================================================================
// OSCReceiver.cpp  –  External OSC listener
// =============================================================================

#include "OSCReceiver.h"
#include "ActionEngine.h"
#include "TalkbackEngine.h"
#include "ConfigManager.h"
#include "OSCHandler.h"
#include "config.h"
#include <Arduino.h>

// ─────────────────────────────────────────────────────────────────────────────

void OSCReceiver::begin() {
    // Reset state
    _extTrigger = false;
    ActionEngine::clearOutput(ACT_SRC_OSC);

    // Close any existing socket
    _udp.stop();
    _udpOpen = false;

    if (!Config.extOscEnabled) {
        Serial.println("[XOSC] Disabled.");
        return;
    }

    if (_udp.begin(Config.extOscPort)) {
        _udpOpen = true;
        Serial.printf("[XOSC] Listening on port %u\n", Config.extOscPort);
    } else {
        Serial.println("[XOSC] UDP begin failed!");
    }
}

// ── Trigger helper ────────────────────────────────────────────────────────────

void OSCReceiver::handleTrigger(bool on) {
    if (on == _extTrigger) return;  // no change
    _extTrigger = on;
    if (on) {
        Serial.println("[XOSC] Trigger ON");
        // Fire trigger-0 actions via OSC source
        ActionEngine::execute(Config.triggers[0].onJson, ACT_SRC_OSC);
    } else {
        Serial.println("[XOSC] Trigger OFF");
        ActionEngine::execute(Config.triggers[0].offJson, ACT_SRC_OSC);
        ActionEngine::clearOutput(ACT_SRC_OSC);
    }
}

// ── Packet parser ─────────────────────────────────────────────────────────────

void OSCReceiver::processIncoming() {
    int sz = _udp.parsePacket();
    if (sz <= 0) return;

    int rd = _udp.read(_rxBuf, sizeof(_rxBuf) - 1);
    if (rd <= 0) return;

    OSCMessage msg = OSCHandler::parse(_rxBuf, (size_t)rd);
    if (!msg.valid) return;

    Serial.printf("[XOSC] Received: %s  typeTag=%c\n",
                  msg.address.c_str(), msg.typeTag ? msg.typeTag : '?');

    // ── /calllight/trigger {int 0|1} ──────────────────────────────────────────
    if (msg.address == XOSC_PATH_TRIGGER) {
        bool on = (msg.typeTag == 'i') ? (msg.intVal != 0)
                                       : (msg.floatVal >= 0.5f);
        handleTrigger(on);

    // ── /calllight/trigger/on ─────────────────────────────────────────────────
    } else if (msg.address == XOSC_PATH_TRIGGER_ON) {
        handleTrigger(true);

    // ── /calllight/trigger/off ────────────────────────────────────────────────
    } else if (msg.address == XOSC_PATH_TRIGGER_OFF) {
        handleTrigger(false);

    // ── /calllight/talkback/a {int 0|1} ──────────────────────────────────────
    } else if (msg.address == XOSC_PATH_TB_A) {
        bool on = (msg.typeTag == 'i') ? (msg.intVal != 0)
                                       : (msg.floatVal >= 0.5f);
        TalkbackEngine::instance().simulateTalkback(true, on);

    // ── /calllight/talkback/b {int 0|1} ──────────────────────────────────────
    } else if (msg.address == XOSC_PATH_TB_B) {
        bool on = (msg.typeTag == 'i') ? (msg.intVal != 0)
                                       : (msg.floatVal >= 0.5f);
        TalkbackEngine::instance().simulateTalkback(false, on);
    }
}

// ── Main loop ─────────────────────────────────────────────────────────────────

void OSCReceiver::loop() {
    if (!Config.extOscEnabled || !_udpOpen) return;
    processIncoming();
}
