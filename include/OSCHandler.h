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
    char    typeTag;        // 'f'=float, 'i'=int32, 's'=string, 'b'=blob, 0=none
    float   floatVal;       // For 'b': float[0] from blob
    int32_t intVal;         // For 'b': number of floats in blob (count header or byteCount/4)
    String  stringVal;
    bool    valid;
    size_t  blobArgOffset;  // For 'b': byte offset of blob arg in raw packet (incl. 4-byte BE size field)
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

    // Build a /batchsubscribe message for a meter-bank subscription
    // (X32/M32 OSC Command Reference, p.7/12).
    // Format: /batchsubscribe ,ssiii <alias> <path> <iStart> <iEnd> <tf>
    //   alias  – OSC address the responses are returned with (e.g. "/m0")
    //   path   – meter bank, e.g. "/meters/0"
    //   iStart – first 0-based float index within the bank
    //   iEnd   – last 0-based float index, INCLUSIVE (e.g. /meters/0: 0..69)
    //   tf     – frequency factor (1 ≈ 50 ms updates)
    // Subscription lasts ~10 s; resend (or /renew ,s <alias>) to keep it alive.
    // NOTE: iEnd is an inclusive index range, NOT a count. /meters/6's
    // per-channel selection is NOT available this way — use a full-bank index
    // range (/meters/0,1,2) and pick the channel's float by index instead.
    static size_t buildBatchSubscribe(const String& alias, const String& path,
                                      int32_t iStart, int32_t iEnd, int32_t tf,
                                      uint8_t* buf, size_t bufLen);

    // Parse a raw UDP buffer into an OSCMessage struct
    static OSCMessage parse(const uint8_t* buf, size_t len);

    // Read a specific float from an X32 meter blob in the raw UDP buffer.
    // Handles both: [LE-count-header][LE floats] and raw [LE floats] (no header).
    // blobArgOffset = OSCMessage::blobArgOffset (points to the 4-byte BE size field).
    // floatIndex    = 0-based channel index within the blob.
    static float extractMeterFloat(const uint8_t* buf, size_t bufLen,
                                    size_t blobArgOffset, uint32_t floatIndex);

private:
    // Write an OSC string (NUL-terminated, 4-byte padded); returns new offset.
    static size_t  writeOSCString(uint8_t* buf, size_t offset,
                                  size_t bufLen, const String& str);
    // Read a 4-byte-padded OSC string into `out`; returns new offset.
    static size_t  readOSCString(const uint8_t* buf, size_t len,
                                 size_t offset, String& out);
    static float   readBEFloat(const uint8_t* p);   // big-endian float (OSC wire order)
    static int32_t readBEInt  (const uint8_t* p);   // big-endian int32 (OSC wire order)
    static float   readLEFloat(const uint8_t* p);   // little-endian float (blob)
    static int32_t readLEInt  (const uint8_t* p);   // little-endian int32 (blob header)
};
