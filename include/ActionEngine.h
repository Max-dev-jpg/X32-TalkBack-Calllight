#pragma once
// =============================================================================
// ActionEngine.h  –  Shared OSC action executor with per-source output bitmask
//
// All talkback, trigger, and external-OSC action lists flow through here so
// the call-light "out" override is tracked per-source and stays consistent.
//
// Sources (ACT_SRC_* from config.h):
//   ACT_SRC_TRIGGER 0  –  mixer-channel trigger
//   ACT_SRC_TB_A    1  –  Talkback A button
//   ACT_SRC_TB_B    2  –  Talkback B button
//   ACT_SRC_OSC     3  –  external OSC receiver
//
// Output bitmask:
//   bit (1 << srcId) is set when an 'out:{s:true}' action fires for that source.
//   isOutputActive() returns true while any bit is set.
//   clearOutput(src) clears that source's bit (call on release/off events).
// =============================================================================

#include <Arduino.h>
#include <WiFiUdp.h>

class ActionEngine {
public:
    // Open the send-only UDP socket.  Call once from setup().
    static void begin();

    // Execute a JSON action array string.
    // srcId  – one of ACT_SRC_* (used to track 'out' overrides per source).
    static void execute(const char* jsonStr, uint8_t srcId);

    // Clear the output-override bit for the given source.
    // Call this when a source's event ends (e.g. talkback released).
    static void clearOutput(uint8_t srcId);

    // Returns true if any source has an active 'out' override.
    static bool isOutputActive() { return _outputMask != 0; }

    // Returns true if a 'forceout' action is active from any source.
    // While true, ALL outputs (triggers, OSC, etc.) are suppressed.
    static bool isForcedOff() { return _suppressMask != 0; }

private:
    static void    sendInt   (const String& addr, int32_t val);   // OSC message with one int arg
    static void    sendNoArg (const String& addr);                // OSC message with no arguments
    static uint8_t channelToSoloID(uint8_t chType, uint8_t chNum);// channel type+number → X32 solo bus id
    static String  buildMutePath  (uint8_t chType, uint8_t chNum);// channel type+number → /…/mix/on path

    static WiFiUDP _udp;
    static bool    _udpReady;
    static uint8_t _outputMask;   // bit per ACT_SRC_* — set by 'out' action
    static uint8_t _suppressMask; // bit per ACT_SRC_* — set by 'forceout'; suppresses ALL outputs
};
