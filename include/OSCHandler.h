#pragma once
// =============================================================================
// OSCHandler.h  –  Lightweight OSC encoder / decoder (no external library)
//
// Implements what is needed for X32/M32 communication:
//   - Build query messages (address, no args)
//   - Build string-arg messages
//   - Build int-arg messages
//   - Build /batchsubscribe messages for per-channel meter subscriptions
//   - Parse responses: float, int, string, blob (X32 LE-float blob format)
// =============================================================================

#include <Arduino.h>

// ── Parsed OSC message ────────────────────────────────────────────────────────
struct OSCMessage {
    String  address;
    char    typeTag;      // 'f'=float, 'i'=int32, 's'=string, 'b'=blob, 0=none
    float   floatVal;     // For 'b': first LE float from blob
    int32_t intVal;       // For 'b': number of floats in blob
    String  stringVal;
    bool    valid;
};

class OSCHandler {
public:
    // Build an OSC query (address only, no arguments)
    static size_t buildQuery(const String& address,
                             uint8_t* buf, size_t bufLen);

    // Build an OSC message with a single string argument
    static size_t buildStringMsg(const String& address,
                                 const String& strArg,
                                 uint8_t* buf, size_t bufLen);

    // Build an OSC message with a single int32 argument
    static size_t buildIntMsg(const String& address, int32_t value,
                              uint8_t* buf, size_t bufLen);

    // Build a /batchsubscribe message for a single-channel meter subscription
    // (X32/M32 OSC Command Reference)
    // Format: /batchsubscribe ,ssiii <alias> /meters/6 <channelId> 0 <tf>
    //   alias     – OSC address of subscription responses (e.g. "/mt0")
    //   channelId – 0-based channel index on /meters/6
    //   tf        – update frames (1 frame = 5 ms; tf=10 → 50 ms updates)
    // Subscription lasts 10 s; renew with buildStringMsg("/renew", alias).
    static size_t buildBatchSubscribe(const String& alias,
                                      int32_t channelId, int32_t tf,
                                      uint8_t* buf, size_t bufLen);

    // Parse a raw UDP buffer into an OSCMessage struct
    static OSCMessage parse(const uint8_t* buf, size_t len);

private:
    static size_t  writeOSCString(uint8_t* buf, size_t offset,
                                  size_t bufLen, const String& str);
    static size_t  readOSCString(const uint8_t* buf, size_t len,
                                 size_t offset, String& out);
    static float   readBEFloat(const uint8_t* p);
    static int32_t readBEInt  (const uint8_t* p);
    static float   readLEFloat(const uint8_t* p);  // little-endian float (blob)
};
