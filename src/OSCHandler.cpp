// =============================================================================
// OSCHandler.cpp  –  Lightweight OSC encode / decode
// =============================================================================

#include "OSCHandler.h"
#include <string.h>

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

// Write a null-terminated string into buf, padded to 4-byte boundary.
// Returns the new write offset.
size_t OSCHandler::writeOSCString(uint8_t* buf, size_t offset,
                                   size_t bufLen, const String& str) {
    size_t slen = str.length();
    if (offset + slen + 1 > bufLen) return offset; // overflow guard
    memcpy(buf + offset, str.c_str(), slen);
    offset += slen;
    // Null terminator + pad to next 4-byte boundary
    do {
        buf[offset++] = 0;
    } while (offset % 4 != 0);
    return offset;
}

// Read a null-terminated string from OSC buf at offset.
// Returns the new read offset (padded to 4-byte boundary).
size_t OSCHandler::readOSCString(const uint8_t* buf, size_t len,
                                  size_t offset, String& out) {
    out = "";
    while (offset < len && buf[offset] != 0) {
        out += (char)buf[offset++];
    }
    // Skip past null + padding
    do { offset++; } while (offset < len && offset % 4 != 0);
    return offset;
}

float OSCHandler::readBEFloat(const uint8_t* p) {
    uint32_t raw = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                   ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
    float f;
    memcpy(&f, &raw, 4);
    return f;
}

int32_t OSCHandler::readBEInt(const uint8_t* p) {
    return (int32_t)(((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                     ((uint32_t)p[2] <<  8) |  (uint32_t)p[3]);
}

// ─────────────────────────────────────────────────────────────────────────────
// Public encode API
// ─────────────────────────────────────────────────────────────────────────────

size_t OSCHandler::buildQuery(const String& address, uint8_t* buf, size_t bufLen) {
    memset(buf, 0, bufLen);
    size_t offset = 0;
    offset = writeOSCString(buf, offset, bufLen, address);
    // Empty type-tag string: ",\0\0\0" (padded to 4 bytes)
    offset = writeOSCString(buf, offset, bufLen, ",");
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

// ─────────────────────────────────────────────────────────────────────────────
// Public decode API
// ─────────────────────────────────────────────────────────────────────────────

OSCMessage OSCHandler::parse(const uint8_t* buf, size_t len) {
    OSCMessage msg;
    msg.valid   = false;
    msg.typeTag = 0;
    msg.floatVal = 0.0f;
    msg.intVal   = 0;

    if (len < 8) return msg;  // too short to be valid

    // Read address
    size_t offset = readOSCString(buf, len, 0, msg.address);
    if (offset >= len) return msg;

    // Read type tag string (starts with ',')
    String typeTag;
    offset = readOSCString(buf, len, offset, typeTag);
    if (typeTag.length() < 1 || typeTag[0] != ',') return msg;

    msg.valid = true;

    // Parse first argument based on type
    if (typeTag.length() < 2) return msg;  // no arguments, still valid

    char firstType = typeTag[1];
    msg.typeTag = firstType;

    switch (firstType) {
        case 'f':
            if (offset + 4 <= len) {
                msg.floatVal = readBEFloat(buf + offset);
            }
            break;
        case 'i':
            if (offset + 4 <= len) {
                msg.intVal = readBEInt(buf + offset);
            }
            break;
        case 's':
            readOSCString(buf, len, offset, msg.stringVal);
            break;
        case 'T':  // boolean true
            msg.intVal = 1;
            break;
        case 'F':  // boolean false
            msg.intVal = 0;
            break;
        default:
            break;
    }

    return msg;
}
