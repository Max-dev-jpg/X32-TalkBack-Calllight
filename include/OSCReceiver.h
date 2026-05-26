#pragma once
// =============================================================================
// OSCReceiver.h  –  External OSC listener for Companion / OSC controller input
//
// Listens on Config.extOscPort for incoming OSC messages and maps them to
// call-light events.  Only active when Config.extOscEnabled is true.
//
// Supported paths:
//   /calllight/trigger      {int 0|1}   set trigger on/off; runs trigger actions
//   /calllight/trigger/on   {}          trigger on  (no argument required)
//   /calllight/trigger/off  {}          trigger off (no argument required)
//   /calllight/talkback/a   {int 0|1}   simulate Talkback A button press/release
//   /calllight/talkback/b   {int 0|1}   simulate Talkback B button press/release
// =============================================================================

#include <Arduino.h>
#include <WiFiUdp.h>

class OSCReceiver {
public:
    static OSCReceiver& instance() {
        static OSCReceiver inst;
        return inst;
    }

    // Open / close UDP socket according to current config.  Safe to call again
    // after a config change (stops old socket first).
    void begin();

    // Must be called every loop() iteration to receive and dispatch messages.
    void loop();

    // True while /calllight/trigger is in the "on" state via OSC.
    // main.cpp ORs this into the overall 'triggered' flag so the call light
    // activates even without a real mixer-channel event.
    bool isExtTriggerActive() const { return _extTrigger; }

private:
    OSCReceiver() {}

    void processIncoming();
    void handleTrigger(bool on);

    WiFiUDP  _udp;
    bool     _udpOpen    = false;
    bool     _extTrigger = false;
    uint8_t  _rxBuf[512];
};
