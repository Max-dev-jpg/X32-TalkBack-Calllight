#pragma once
// =============================================================================
// OSCHandler.h  –  Lightweight OSC encoder / decoder (no external library)
//
// Implements only what is needed for X32/M32 communication:
//   - Build query messages  (address, no arguments)
//   - Build string-argument messages  (/meters with path string)
//   - Parse responses that carry one float, one int, or one string argument
// =============================================================================

#include <Arduino.h>

// ── Parsed OSC message ────────────────────────────────────────────────────────
struct OSCMessage {
    String  address;
    char    typeTag;      // 'f'=float, 'i'=int32, 's'=string, 'b'=blob, 0=none
    float   floatVal;
    int32_t intVal;
    String  stringVal;
    bool    valid;
};

class OSCHandler {
public:
    // Build an OSC query (address only, no arguments) → raw bytes
    static size_t buildQuery(const String& address, uint8_t* buf, size_t bufLen);

    // Build an OSC message with a single string argument (e.g. /meters)
    static size_t buildStringMsg(const String& address,
                                 const String& strArg,
                                 uint8_t* buf, size_t bufLen);

    // Build an OSC message with a single int32 argument (for solo on/off etc.)
    static size_t buildIntMsg(const String& address, int32_t value,
                              uint8_t* buf, size_t bufLen);

    // Parse a raw UDP buffer into an OSCMessage struct
    static OSCMessage parse(const uint8_t* buf, size_t len);

private:
    // Pad a string into buf at offset, returning new offset (4-byte aligned)
    static size_t writeOSCString(uint8_t* buf, size_t offset,
                                 size_t bufLen, const String& str);

    // Read a null-terminated string from OSC buffer at given offset
    static size_t readOSCString(const uint8_t* buf, size_t len,
                                size_t offset, String& out);

    // Read big-endian float from 4 bytes
    static float readBEFloat(const uint8_t* p);

    // Read big-endian int32 from 4 bytes
    static int32_t readBEInt(const uint8_t* p);
};
