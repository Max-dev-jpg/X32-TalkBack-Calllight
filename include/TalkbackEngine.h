#pragma once
// =============================================================================
// TalkbackEngine.h  –  Monitors X32/M32 talkback buttons; executes action lists
//
// Architecture
// ────────────
//  • Polls /-stat/talk/A and /-stat/talk/B via its own UDP socket (port 10025).
//  • Each button (A / B) has independent ON and OFF action lists stored as JSON
//    arrays in Config.tbAOnJson / tbAOffJson / tbBOnJson / tbBOffJson.
//  • Action execution is delegated to ActionEngine (shared with trigger/OSC).
//  • simulateTalkback() lets the external OSC receiver simulate button presses.
// =============================================================================

#include <Arduino.h>
#include <WiFiUdp.h>

class TalkbackEngine {
public:
    // Singleton accessor.
    static TalkbackEngine& instance() {
        static TalkbackEngine inst;
        return inst;
    }

    void begin();   // open the UDP socket and subscribe to the talkback state
    void loop();    // poll talkback state, re-subscribe, and run edge actions

    // Live state accessors (used by WebServerManager for status JSON).
    // Reports the effective output state, so with A/B linked both read active
    // when either console button is pressed.
    bool isTalkbackAActive() const { return _outA; }
    bool isTalkbackBActive() const { return _outB; }

    // Simulate a talkback button press/release from an external source (OSC receiver).
    // isA=true → button A, isA=false → button B; active=true → pressed, false → released.
    void simulateTalkback(bool isA, bool active);

private:
    TalkbackEngine() {}

    // UDP helpers (send only – uses own socket so it works even without ActionEngine)
    void sendQuery (const String& address);                 // address-only query
    void sendInt   (const String& address, int32_t value);  // message with one int arg
    void sendNoArg (const String& address);                 // message with no arguments

    void processIncoming();     // parse queued UDP packets → update _stateA/_stateB

    void updateOutputs();           // recompute effective A/B outputs from raw button states
    void applyOutput(bool isA, bool desired);  // run ON/OFF action list on an output edge

    WiFiUDP  _udp;
    bool     _udpOpen = false;

    // Raw console button states.
    bool     _stateA  = false;
    bool     _stateB  = false;
    // Effective output states actually driven (differ from raw when A/B linked).
    bool     _outA    = false;
    bool     _outB    = false;

    uint32_t _lastXRemoteMs = 0;

    uint8_t  _txBuf[256];
    uint8_t  _rxBuf[512];
};
