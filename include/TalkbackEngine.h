#pragma once
// =============================================================================
// TalkbackEngine.h  –  Monitors X32/M32 talkback buttons; sends solo commands
//
// Mirrors the functionality of the "X32 Talkback Utility Tool":
//   - Polls /-stat/talk/A and /-stat/talk/B via its own UDP socket
//   - On TB_ON: optionally clears all solos, sends solo to configured channel,
//               executes custom OSC commands
//   - On TB_OFF: un-solos channel, executes custom OSC commands
//   - Exposes live A/B talkback state for web UI display
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

    // Live state accessors
    bool isTalkbackAActive() const { return _stateA; }
    bool isTalkbackBActive() const { return _stateB; }

private:
    TalkbackEngine() {}

    // UDP send helpers
    void sendQuery(const String& address);
    void sendInt  (const String& address, int32_t value);
    void sendNoArg(const String& address);

    // Process incoming UDP response
    void processIncoming();

    // Compute X32 solo-bus ID from channel type + number
    uint8_t soloID() const;

    // React to a talkback state change
    void onTalkbackOn();
    void onTalkbackOff();

    WiFiUDP  _udp;
    bool     _udpOpen = false;

    bool     _stateA = false;
    bool     _stateB = false;

    uint32_t _lastPollMs     = 0;
    uint32_t _lastXRemoteMs  = 0;

    uint8_t  _txBuf[256];
    uint8_t  _rxBuf[512];
};
