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
    // Reset all trigger states
    for (uint8_t n = 0; n < MAX_TRIGGERS; n++) {
        _extTrigger[n]   = false;
        _pulseUntilMs[n] = 0;
    }
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

// ── Per-trigger handler ───────────────────────────────────────────────────────

void OSCReceiver::handleTriggerN(uint8_t n, bool on) {
    if (n >= MAX_TRIGGERS) return;
    if (on == _extTrigger[n]) return;   // no change
    _extTrigger[n] = on;

    if (on) {
        Serial.printf("[XOSC] Trigger %u ON\n", n + 1);
        ActionEngine::execute(Config.triggers[n].onJson, ACT_SRC_OSC);
    } else {
        Serial.printf("[XOSC] Trigger %u OFF\n", n + 1);
        ActionEngine::execute(Config.triggers[n].offJson, ACT_SRC_OSC);
        // Only clear the output bitmask once no OSC triggers remain active
        if (!isExtTriggerActive()) ActionEngine::clearOutput(ACT_SRC_OSC);
    }
}

// ── Pulse (momentary) handler ─────────────────────────────────────────────────

void OSCReceiver::pulseTriggerN(uint8_t n, uint32_t ms) {
    if (n >= MAX_TRIGGERS) return;
    _pulseUntilMs[n] = millis() + ms;
    if (!_extTrigger[n]) {
        // Fire on — loop() will fire off when the timer expires
        _extTrigger[n] = true;
        Serial.printf("[XOSC] Trigger %u PULSE %u ms\n", n + 1, ms);
        ActionEngine::execute(Config.triggers[n].onJson, ACT_SRC_OSC);
    } else {
        // Retriggered while already on: extend the deadline
        Serial.printf("[XOSC] Trigger %u PULSE extended to %u ms\n", n + 1, ms);
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

    const String& addr = msg.address;

    // ── Talkback ──────────────────────────────────────────────────────────────
    if (addr == XOSC_PATH_TB_A) {
        bool on = (msg.typeTag == 'i') ? (msg.intVal != 0) : (msg.floatVal >= 0.5f);
        TalkbackEngine::instance().simulateTalkback(true, on);
        return;
    }
    if (addr == XOSC_PATH_TB_B) {
        bool on = (msg.typeTag == 'i') ? (msg.intVal != 0) : (msg.floatVal >= 0.5f);
        TalkbackEngine::instance().simulateTalkback(false, on);
        return;
    }

    // ── Trigger paths ─────────────────────────────────────────────────────────
    if (!addr.startsWith("/calllight/trigger")) return;

    // Legacy no-number paths  →  Trigger 1 (index 0)
    if (addr == XOSC_PATH_TRIGGER) {
        bool on = (msg.typeTag == 'i') ? (msg.intVal != 0) : (msg.floatVal >= 0.5f);
        handleTriggerN(0, on);
        return;
    }
    if (addr == XOSC_PATH_TRIGGER_ON)  { handleTriggerN(0, true);  return; }
    if (addr == XOSC_PATH_TRIGGER_OFF) { handleTriggerN(0, false); return; }

    // Numbered paths: /calllight/trigger/N[/action]
    // "/calllight/trigger/" is 19 chars; next char is the trigger number 1-4
    if (addr.length() < 20) return;
    char nc = addr.charAt(19);
    if (nc < '1' || nc > '4') return;
    uint8_t n = (uint8_t)(nc - '1');          // 0-based index

    const String suffix = addr.substring(20); // everything after the digit

    if (suffix.length() == 0) {
        // /calllight/trigger/N  →  set on/off
        bool on = (msg.typeTag == 'i') ? (msg.intVal != 0) : (msg.floatVal >= 0.5f);
        handleTriggerN(n, on);
    } else if (suffix == "/on") {
        handleTriggerN(n, true);
    } else if (suffix == "/off") {
        handleTriggerN(n, false);
    } else if (suffix == "/pulse") {
        uint32_t ms = XOSC_PULSE_DEFAULT_MS;
        if      (msg.typeTag == 'i' && msg.intVal   > 0) ms = (uint32_t)msg.intVal;
        else if (msg.typeTag == 'f' && msg.floatVal > 0) ms = (uint32_t)msg.floatVal;
        pulseTriggerN(n, ms);
    }
}

// ── Main loop ─────────────────────────────────────────────────────────────────

void OSCReceiver::loop() {
    if (!Config.extOscEnabled || !_udpOpen) return;

    processIncoming();

    // Auto-release pulsed triggers whose timer has expired
    uint32_t now = millis();
    for (uint8_t n = 0; n < MAX_TRIGGERS; n++) {
        if (_extTrigger[n] && _pulseUntilMs[n] > 0 && now >= _pulseUntilMs[n]) {
            _pulseUntilMs[n] = 0;
            handleTriggerN(n, false);
        }
    }
}
