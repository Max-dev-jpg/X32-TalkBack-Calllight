// =============================================================================
// OSCHandler.cpp  –  Lightweight OSC encode / decode
// =============================================================================

#include "OSCHandler.h"
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

size_t OSCHandler::writeOSCString(uint8_t* buf, size_t offset,
                                   size_t bufLen, const String& str) {
  for (size_t i = 0; i < str.length() && offset < bufLen; ++i) {
    buf[offset++] = str[i];
  }
  if (offset < bufLen) {
    buf[offset++] = '\0';
  }
  while ((offset % 4) != 0 && offset < bufLen) {
    buf[offset++] = '\0';
  }
  return offset;
}

size_t OSCHandler::readOSCString(const uint8_t* buf, size_t len,
                                  size_t offset, String& out) {
    out = "";
    while (offset < len && buf[offset] != 0) {
        out += (char)buf[offset++];
    }
    do { offset++; } while (offset < len && offset % 4 != 0);
    return offset;
}

float OSCHandler::readBEFloat(const uint8_t* p) {
    uint32_t raw = readBEInt(p);
    float value; 
    memcpy(&value, &raw, sizeof(value)); 
    return value;
}

int32_t OSCHandler::readBEInt(const uint8_t* p) {
    return (int32_t)(((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                     ((uint32_t)p[2] <<  8) |  (uint32_t)p[3]);
}

// X32/M32 blob data is little-endian floats
float OSCHandler::readLEFloat(const uint8_t* p) {
    uint32_t raw = readLEInt(p);
    float value; 
    memcpy(&value, &raw, sizeof(value)); 
    return value;
}

// X32/M32 blob header: float count is a little-endian int32
int32_t OSCHandler::readLEInt(const uint8_t* p) {
    return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                     ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

// ─────────────────────────────────────────────────────────────────────────────
// Public encode API
// ─────────────────────────────────────────────────────────────────────────────

size_t OSCHandler::buildQuery(const String& address, uint8_t* buf, size_t bufLen) {
    memset(buf, 0, bufLen);
    size_t offset = 0;
    offset = writeOSCString(buf, offset, bufLen, address);
    offset = writeOSCString(buf, offset, bufLen, ",");
    return offset;
}

size_t OSCHandler::buildIntMsg(const String& address, int32_t value,
                               uint8_t* buf, size_t bufLen) {
    memset(buf, 0, bufLen);
    size_t offset = 0;
    offset = writeOSCString(buf, offset, bufLen, address);
    offset = writeOSCString(buf, offset, bufLen, ",i");
    if (offset + 4 <= bufLen) {
        buf[offset++] = (uint8_t)((value >> 24) & 0xFF);
        buf[offset++] = (uint8_t)((value >> 16) & 0xFF);
        buf[offset++] = (uint8_t)((value >>  8) & 0xFF);
        buf[offset++] = (uint8_t)( value         & 0xFF);
    }
    return offset;
}

size_t OSCHandler::buildStringMsg(const String& address,
                                   const String& strArg,
                                   uint8_t* buf, size_t bufLen) {
    memset(buf, 0, bufLen);
    size_t offset = 0;
    offset = writeOSCString(buf, offset, bufLen, address);
    offset = writeOSCString(buf, offset, bufLen, ",s");
    offset = writeOSCString(buf, offset, bufLen, strArg);
    return offset;
}

// Build a "/batchsubscribe ,ssiii <alias> <path> <iStart> <iEnd> <tf>" request.
//
// Per the X32/M32 OSC protocol (doc p.7/12) this subscribes to a contiguous
// index range [iStart..iEnd] (INCLUSIVE) of a meter bank. The console streams an
// OSC blob — addressed to <alias> — containing (iEnd - iStart + 1) little-endian
// floats, about every 50 ms for ~10 s. <tf> is the frequency factor.
//
// Example from the doc: "/x_meters_0 /meters/0 0 69 1" → blob of 70 floats.
//
// IMPORTANT: iEnd is an inclusive index, NOT a count. The per-channel selection
// of /meters/6 does NOT work through /batchsubscribe (it has no channel-id slot,
// and a single global selection is shared across all subscribers — which made
// multiple /meters/6 subscriptions all report the same channel). Subscribe to a
// full bank (/meters/0,1,2) instead and pick each channel's float by index.
//
// The subscription must be renewed (resend it, or "/renew ,s <alias>").
size_t OSCHandler::buildBatchSubscribe(const String& alias, const String& path,
                                        int32_t iStart, int32_t iEnd, int32_t tf,
                                        uint8_t* buf, size_t bufLen) {
    memset(buf, 0, bufLen);
    size_t offset = 0;
    offset = writeOSCString(buf, offset, bufLen, "/batchsubscribe");
    offset = writeOSCString(buf, offset, bufLen, ",ssiii");
    offset = writeOSCString(buf, offset, bufLen, alias);
    offset = writeOSCString(buf, offset, bufLen, path);

    auto writeInt = [&](int32_t value) {
        if (offset + 4 <= bufLen) {
            buf[offset++] = (uint8_t)((value >> 24) & 0xFF);
            buf[offset++] = (uint8_t)((value >> 16) & 0xFF);
            buf[offset++] = (uint8_t)((value >>  8) & 0xFF);
            buf[offset++] = (uint8_t)( value        & 0xFF);
            }
    };
    writeInt(iStart);
    writeInt(iEnd);    // inclusive last index
    writeInt(tf);      // frequency factor (1 ≈ 50 ms updates)
    return offset;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public decode API
// ─────────────────────────────────────────────────────────────────────────────

OSCMessage OSCHandler::parse(const uint8_t* buf, size_t len) {
    OSCMessage msg;
    msg.valid       = false;
    msg.typeTag     = 0;
    msg.floatVal    = 0.0f;
    msg.intVal      = 0;
    msg.blobArgOffset = 0;

    if (len < 8) return msg;

    size_t offset = readOSCString(buf, len, 0, msg.address);
    if (offset >= len) return msg;

    String typeTag;
    offset = readOSCString(buf, len, offset, typeTag);
    if (typeTag.length() < 1 || typeTag[0] != ',') return msg;

    msg.valid = true;
    if (typeTag.length() < 2) return msg;

    char firstType = typeTag[1];
    msg.typeTag = firstType;

    switch (firstType) {
        case 'f':
            if (offset + 4 <= len)
                msg.floatVal = readBEFloat(buf + offset);
            break;

        case 'i':
            if (offset + 4 <= len)
                msg.intVal = readBEInt(buf + offset);
            break;

        case 's':
            readOSCString(buf, len, offset, msg.stringVal);
            break;

        case 'T': msg.intVal = 1; break;
        case 'F': msg.intVal = 0; break;

        case 'b':
            // X32/M32 meter blob — two possible formats depending on firmware:
            //   Format A (with LE count header):
            //     [offset + 0..3]: blob byte count (BE int32)
            //     [offset + 4..7]: float count     (LE int32, X32-specific header)
            //     [offset + 8..]: LE float32 values
            //   Format B (raw, no count header):
            //     [offset + 0..3]: blob byte count (BE int32)
            //     [offset + 4..]: LE float32 values directly
            // extractMeterFloat() handles both; floatVal and intVal are set for
            // quick access to the first float and the total float count.
            msg.blobArgOffset = offset;  // save raw position for extractMeterFloat()
            if (offset + 4 <= len) {
                uint32_t byteCount = (uint32_t)readBEInt(buf + offset);
                if (byteCount >= 8 && offset + 8 <= len) {
                    // Try Format A: check if the 4 bytes after byte-count look like a LE count
                    uint32_t maybeCount = (uint32_t)readLEInt(buf + offset + 4);
                    if (maybeCount > 0 && byteCount == 4 + maybeCount * 4
                            && offset + 8 + maybeCount * 4 <= len) {
                        // Format A confirmed
                        msg.floatVal = readLEFloat(buf + offset + 8);
                        msg.intVal   = (int32_t)maybeCount;
                        break;
                    }
                }
                // Format B (or Format A failed sanity): raw LE floats
                if (byteCount >= 4 && offset + 4 + byteCount <= len) {
                    msg.floatVal = readLEFloat(buf + offset + 4);
                    msg.intVal   = (int32_t)(byteCount / 4);
                }
            }
            break;

        default:
            break;
    }

    return msg;
}

// ─────────────────────────────────────────────────────────────────────────────
// extractMeterFloat — read one float value (by index) from an X32 meter blob
// ─────────────────────────────────────────────────────────────────────────────

float OSCHandler::extractMeterFloat(const uint8_t* buf, size_t bufLen,
                                     size_t blobArgOffset, uint32_t floatIndex) {
    if (blobArgOffset + 4 > bufLen) return 0.0f;

    uint32_t byteCount = (uint32_t)readBEInt(buf + blobArgOffset);

    // Try Format A: [BE byte-count][LE float-count][LE floats...]
    if (byteCount >= 8 && blobArgOffset + 8 <= bufLen) {
        uint32_t floatCount = (uint32_t)readLEInt(buf + blobArgOffset + 4);
        if (floatCount > 0 && byteCount == 4 + floatCount * 4) {
            uint32_t idx = (floatIndex < floatCount) ? floatIndex : 0;
            size_t pos = blobArgOffset + 8 + idx * 4;
            if (pos + 4 <= bufLen) return readLEFloat(buf + pos);
        }
    }

    // Format B: [BE byte-count][LE floats directly]
    if (byteCount >= 4 && blobArgOffset + 4 + byteCount <= bufLen) {
        uint32_t floatCount = byteCount / 4;
        uint32_t idx = (floatIndex < floatCount) ? floatIndex : 0;
        size_t pos = blobArgOffset + 4 + idx * 4;
        if (pos + 4 <= bufLen) return readLEFloat(buf + pos);
    }

    return 0.0f;
}
