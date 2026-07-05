// =============================================================================
// ActionEngine.cpp  –  Shared OSC action executor
// =============================================================================

#include "ActionEngine.h"
#include "ConfigManager.h"
#include "OSCHandler.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>

// ── Static member definitions ─────────────────────────────────────────────────
WiFiUDP ActionEngine::_udp;
bool    ActionEngine::_udpReady    = false;
uint8_t ActionEngine::_outputMask  = 0;
uint8_t ActionEngine::_suppressMask = 0;

// ─────────────────────────────────────────────────────────────────────────────

// Open the shared send-only UDP socket used to push OSC actions to the mixer.
void ActionEngine::begin() {
    _udp.stop();
    _udpReady = _udp.begin(ACT_SEND_PORT);
    if (_udpReady) {
        DBG_PRINTF("[Action] Send socket ready on port %d\n", ACT_SEND_PORT);
    } else {
        DBG_PRINTLN("[Action] Send socket FAILED!");
    }
}

// ── UDP helpers ───────────────────────────────────────────────────────────────

void ActionEngine::sendInt(const String& addr, int32_t val) {
    if (!_udpReady) return;
    uint8_t buf[256];
    size_t len = OSCHandler::buildIntMsg(addr, val, buf, sizeof(buf));
    _udp.beginPacket(Config.mixerIP, Config.oscTxPort);
    _udp.write(buf, len);
    _udp.endPacket();
}

void ActionEngine::sendNoArg(const String& addr) {
    if (!_udpReady) return;
    uint8_t buf[256];
    size_t len = OSCHandler::buildQuery(addr, buf, sizeof(buf));
    _udp.beginPacket(Config.mixerIP, Config.oscTxPort);
    _udp.write(buf, len);
    _udp.endPacket();
}

// ── Channel helpers ───────────────────────────────────────────────────────────

uint8_t ActionEngine::channelToSoloID(uint8_t chType, uint8_t chNum) {
    switch (chType) {
        case CH_INPUT:  return SOLO_OFFSET_INPUT  + chNum;
        case CH_AUXIN:  return SOLO_OFFSET_AUXIN  + chNum;
        case CH_FXRTN:  return SOLO_OFFSET_FXRTN  + chNum;
        case CH_BUS:    return SOLO_OFFSET_BUS     + chNum;
        case CH_MATRIX: return SOLO_OFFSET_MATRIX  + chNum;
        case CH_MAIN:   return SOLO_ID_MAIN_LR;
        case CH_MONO:   return SOLO_ID_MAIN_MONO;
        case CH_DCA:    return SOLO_OFFSET_DCA     + chNum;
        default:        return chNum;
    }
}

String ActionEngine::buildMutePath(uint8_t chType, uint8_t chNum) {
    char buf[36];
    switch (chType) {
        case CH_INPUT:  snprintf(buf, sizeof(buf), "/ch/%02u/mix/on",    chNum); break;
        case CH_AUXIN:  snprintf(buf, sizeof(buf), "/auxin/%02u/mix/on", chNum); break;
        case CH_FXRTN:  snprintf(buf, sizeof(buf), "/fxrtn/%02u/mix/on", chNum); break;
        case CH_BUS:    snprintf(buf, sizeof(buf), "/bus/%02u/mix/on",   chNum); break;
        case CH_MATRIX: snprintf(buf, sizeof(buf), "/mtx/%02u/mix/on",   chNum); break;
        case CH_DCA:    snprintf(buf, sizeof(buf), "/dca/%u/on",         chNum); break;
        case CH_MAIN:   strlcpy(buf, "/main/st/mix/on", sizeof(buf));            break;
        case CH_MONO:   strlcpy(buf, "/main/m/mix/on",  sizeof(buf));            break;
        default:        buf[0] = '\0';                                            break;
    }
    return String(buf);
}

// ── Action executor ───────────────────────────────────────────────────────────

// Parse a JSON action array and run each action (solo/mute/osc/out/forceout…),
// tracking 'out'/'forceout' overrides under the given source's bit.
void ActionEngine::execute(const char* jsonStr, uint8_t srcId) {
    if (!jsonStr || jsonStr[0] == '\0') return;

    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, jsonStr);
    if (err) {
        DBG_PRINTF("[Action] JSON parse error (src=%u): %s\n", srcId, err.c_str());
        return;
    }

    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject act : arr) {
        const char* t = act["t"] | "";

        if (strcmp(t, "clearSolo") == 0) {
            sendInt(TB_CLEARSOLO_PATH, 1);
            DBG_PRINTLN("[Action]   -> clearSolo");

        } else if (strcmp(t, "solo") == 0) {
            uint8_t id = channelToSoloID(act["ct"] | (uint8_t)0,
                                          act["cn"] | (uint8_t)1);
            char idStr[3];
            snprintf(idStr, sizeof(idStr), "%02u", id);
            sendInt(String(TB_SOLOSW_BASE) + idStr, 1);
            DBG_PRINTF("[Action]   -> solo id=%u\n", id);

        } else if (strcmp(t, "unsolo") == 0) {
            uint8_t id = channelToSoloID(act["ct"] | (uint8_t)0,
                                          act["cn"] | (uint8_t)1);
            char idStr[3];
            snprintf(idStr, sizeof(idStr), "%02u", id);
            sendInt(String(TB_SOLOSW_BASE) + idStr, 0);
            DBG_PRINTF("[Action]   -> unsolo id=%u\n", id);

        } else if (strcmp(t, "mute") == 0) {
            String path = buildMutePath(act["ct"] | (uint8_t)0,
                                         act["cn"] | (uint8_t)1);
            if (path.length()) sendInt(path, 0);   // 0 = muted on X32/M32
            DBG_PRINTF("[Action]   -> mute %s\n", path.c_str());

        } else if (strcmp(t, "unmute") == 0) {
            String path = buildMutePath(act["ct"] | (uint8_t)0,
                                         act["cn"] | (uint8_t)1);
            if (path.length()) sendInt(path, 1);   // 1 = active/unmuted
            DBG_PRINTF("[Action]   -> unmute %s\n", path.c_str());

        } else if (strcmp(t, "osc") == 0) {
            const char* p = act["p"] | "";
            int32_t v     = act["v"] | (int32_t)0;
            if (p[0] != '\0') {
                sendInt(String(p), v);
                DBG_PRINTF("[Action]   -> osc %s = %d\n", p, v);
            }

        } else if (strcmp(t, "out") == 0) {
            bool s = act["s"] | false;
            if (s) {
                _outputMask |=  (1u << srcId);
            } else {
                _outputMask &= ~(1u << srcId);
            }
            DBG_PRINTF("[Action]   -> output src=%u %s\n", srcId, s ? "ON" : "OFF");

        } else if (strcmp(t, "forceout") == 0) {
            // Suppress ALL output sources while active — overrides triggers and OSC
            _suppressMask |= (1u << srcId);
            DBG_PRINTF("[Action]   -> forceout src=%u (all outputs suppressed)\n", srcId);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────

// Clear a source's 'out' and 'forceout' bits (call when its event ends).
void ActionEngine::clearOutput(uint8_t srcId) {
    _outputMask   &= ~(1u << srcId);
    _suppressMask &= ~(1u << srcId);
}
