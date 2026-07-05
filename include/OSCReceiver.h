#pragma once
// =============================================================================
// OSCReceiver.h  –  External OSC listener for Companion / OSC controller input
//
// Listens on Config.extOscPort for incoming OSC messages and maps them to
// call-light events.  Only active when Config.extOscEnabled is true.
//
// Supported paths (N = 1-4):
//   /calllight/trigger         {int 0|1}  legacy — controls Trigger 1
//   /calllight/trigger/on                 legacy — Trigger 1 on
//   /calllight/trigger/off                legacy — Trigger 1 off
//   /calllight/trigger/N       {int 0|1}  set Trigger N on/off; runs its actions
//   /calllight/trigger/N/on               Trigger N on
//   /calllight/trigger/N/off              Trigger N off
//   /calllight/trigger/N/pulse {int ms?}  momentary: on then auto-off after ms
//   /calllight/talkback/a      {int 0|1}  simulate Talkback A
//   /calllight/talkback/b      {int 0|1}  simulate Talkback B
// =============================================================================

#include <Arduino.h>
#include <WiFiUdp.h>
#include "config.h"

class OSCReceiver {
public:
    // Singleton accessor.
    static OSCReceiver& instance() {
        static OSCReceiver inst;
        return inst;
    }

    // Open / close UDP socket according to current config.  Safe to call again
    // after a config change (stops old socket first).
    void begin();

    // Must be called every loop() iteration to receive and dispatch messages.
    void loop();

    // True if any trigger is currently active via external OSC.
    // main.cpp ORs this into the overall 'triggered' flag.
    bool isExtTriggerActive() const {
        for (uint8_t n = 0; n < MAX_TRIGGERS; n++) if (_extTrigger[n]) return true;
        return false;
    }
    // Per-trigger query.
    bool isExtTriggerActive(uint8_t n) const {
        return (n < MAX_TRIGGERS) ? _extTrigger[n] : false;
    }
    // millis() of the trigger's most recent ON edge (0 if not active) — used by
    // main.cpp for PRIO_NEWEST LED color selection, same as TriggerManager.
    uint32_t getTriggerStartMs(uint8_t n) const {
        return (n < MAX_TRIGGERS) ? _startMs[n] : 0;
    }

private:
    OSCReceiver() {}

    void processIncoming();   // read queued UDP packets and dispatch to handlers

    // Set trigger n on/off and fire its configured actions.
    void handleTriggerN(uint8_t n, bool on);

    // Momentary: fire trigger n on, then auto-off after `ms` milliseconds.
    void pulseTriggerN(uint8_t n, uint32_t ms);

    WiFiUDP  _udp;
    bool     _udpOpen                    = false;
    bool     _extTrigger[MAX_TRIGGERS]   = {};
    uint32_t _startMs[MAX_TRIGGERS]      = {};   // millis() of each trigger's ON edge
    uint32_t _pulseUntilMs[MAX_TRIGGERS] = {};
    uint8_t  _rxBuf[512];
};
