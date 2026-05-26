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
    size_t slen = str.length();
    if (offset + slen + 1 > bufLen) return offset;
    memcpy(buf + offset, str.c_str(), slen);
    offset += slen;
    do { buf[offset++] = 0; } while (offset % 4 != 0);
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
    uint32_t raw = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                   ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
    float f; memcpy(&f, &raw, 4); return f;
}

int32_t OSCHandler::readBEInt(const uint8_t* p) {
    return (int32_t)(((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                     ((uint32_t)p[2] <<  8) |  (uint32_t)p[3]);
}

// X32/M32 blob data is little-endian floats
float OSCHandler::readLEFloat(const uint8_t* p) {
    uint32_t raw = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                   ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    float f; memcpy(&f, &raw, 4); return f;
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

// /batchsubscribe ,ssiii  <alias> /meters/6 <channelId> 0 <tf>
// Per X32/M32 OSC Command Reference: subscribes to a single channel on
// /meters/6. Responses arrive as OSC messages addressed to <alias>.
// The subscription expires after 10 s; renew with /renew ,s <alias>.
size_t OSCHandler::buildBatchSubscribe(const String& alias,
                                        int32_t channelId, int32_t tf,
                                        uint8_t* buf, size_t bufLen) {
    memset(buf, 0, bufLen);
    size_t offset = 0;
    offset = writeOSCString(buf, offset, bufLen, "/batchsubscribe");
    offset = writeOSCString(buf, offset, bufLen, ",ssiii");
    offset = writeOSCString(buf, offset, bufLen, alias);
    offset = writeOSCString(buf, offset, bufLen, "/meters/6");

    auto writeInt = [&](int32_t v) {
        if (offset + 4 <= bufLen) {
            buf[offset++] = (uint8_t)((v >> 24) & 0xFF);
            buf[offset++] = (uint8_t)((v >> 16) & 0xFF);
            buf[offset++] = (uint8_t)((v >>  8) & 0xFF);
            buf[offset++] = (uint8_t)( v         & 0xFF);
        }
    };
    writeInt(channelId);
    writeInt(0);       // skip = 0 (contiguous channel selection)
    writeInt(tf);      // tf frames (1 frame = 5 ms; tf=10 → 50 ms)
    return offset;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public decode API
// ─────────────────────────────────────────────────────────────────────────────

OSCMessage OSCHandler::parse(const uint8_t* buf, size_t len) {
    OSCMessage msg;
    msg.valid    = false;
    msg.typeTag  = 0;
    msg.floatVal = 0.0f;
    msg.intVal   = 0;

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
            // X32/M32 blob format (per OSC Command Reference PDF):
            //   [offset + 0..3]: blob byte count (big-endian int32)
            //   [offset + 4..7]: float count     (little-endian int32)
            //   [offset + 8..]: LE 32-bit floats
            if (offset + 8 <= len) {
                // blob byte count (BE) — we use it only for bounds checking
                uint32_t byteCount  = (uint32_t)readBEInt(buf + offset);
                uint32_t floatCount = (uint32_t)readBEInt(buf + offset + 4);
                // Sanity: floatCount must fit inside byteCount
                if (floatCount > 0 && byteCount >= 4 + floatCount * 4) {
                    size_t floatStart = offset + 8;
                    if (floatStart + 4 <= len)
                        msg.floatVal = readLEFloat(buf + floatStart);
                }
                msg.intVal = (int32_t)floatCount; // caller can iterate more floats
            }
            break;

        default:
            break;
    }

    return msg;
}
