#pragma once
// =============================================================================
// TalkbackEngine.h  –  Monitors X32/M32 talkback buttons; executes action lists
//
// Architecture
// ────────────
//  • Polls /-stat/talk/A and /-stat/talk/B via its own UDP socket (port 10025).
//  • Each button (A / B) has independent ON and OFF action lists stored as JSON
//    arrays in Config.tbAOnJson / tbAOffJson / tbBOnJson / tbBOffJson.
//  • Supported action types ("t" field in each action object):
//      clearSolo           – sends /-action/clearsolo
//      solo   {ct, cn}     – solos a channel   (/-stat/solosw/{id} = 1)
//      unsolo {ct, cn}     – unsolos a channel (/-stat/solosw/{id} = 0)
//      mute   {ct, cn}     – mutes a channel   (mix/on = 0)
//      unmute {ct, cn}     – unmutes a channel (mix/on = 1)
//      osc    {p, v}       – sends arbitrary OSC int command
//      out    {s}          – forces the call-light output (s=true/false)
//  • isOutputActive() lets main.cpp OR the talkback output into 'triggered'.
// =============================================================================

#include <Arduino.h>
#include <WiFiUdp.h>

class TalkbackEngine {
public:
    static TalkbackEngine& instance() {
        static TalkbackEngine inst;
        return inst;
    }

    void begin();
    void loop();

    // Live state accessors (used by WebServerManager for status JSON)
    bool isTalkbackAActive() const { return _stateA; }
    bool isTalkbackBActive() const { return _stateB; }

    // True when an 'out' action has forced the call-light on;
    // main.cpp ORs this into the trigger-logic 'triggered' flag.
    bool isOutputActive() const { return _outputActive; }

private:
    TalkbackEngine() {}

    // UDP helpers
    void sendQuery (const String& address);
    void sendInt   (const String& address, int32_t value);
    void sendNoArg (const String& address);

    // Incoming UDP parser
    void processIncoming();

    // Execute a JSON action list
    void executeActions(const char* jsonStr);

    // React to a talkback state change
    void onTalkbackOn (bool isA);
    void onTalkbackOff(bool isA);

    // Compute X32/M32 solo-bus ID from channel type + number (1-based)
    static uint8_t channelToSoloID(uint8_t chType, uint8_t chNum);

    // Build the OSC mute-state path (mix/on or dca/on) for a channel
    static String buildMutePath(uint8_t chType, uint8_t chNum);

    WiFiUDP  _udp;
    bool     _udpOpen      = false;

    bool     _stateA       = false;
    bool     _stateB       = false;
    bool     _outputActive = false;  // set by 'out' actions

    uint32_t _lastPollMs    = 0;
    uint32_t _lastXRemoteMs = 0;

    uint8_t  _txBuf[256];
    uint8_t  _rxBuf[512];
};
