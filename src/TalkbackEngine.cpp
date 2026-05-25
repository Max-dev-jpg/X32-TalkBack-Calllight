// =============================================================================
// TalkbackEngine.cpp
// =============================================================================

#include "TalkbackEngine.h"
#include "ConfigManager.h"
#include "OSCHandler.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>

// ─────────────────────────────────────────────────────────────────────────────

void TalkbackEngine::begin() {
    _stateA       = false;
    _stateB       = false;
    _outputActive = false;

    if (!Config.tbEnabled) return;

    // Re-open UDP (stop first so begin() is safe on config change)
    _udp.stop();
    _udpOpen = false;

    if (_udp.begin(TB_RX_PORT)) {
        _udpOpen = true;
        Serial.printf("[TB] UDP listening on port %d\n", TB_RX_PORT);
    } else {
        Serial.println("[TB] UDP begin failed!");
    }
}

// ── UDP helpers ───────────────────────────────────────────────────────────────

void TalkbackEngine::sendQuery(const String& address) {
    if (!_udpOpen) return;
    size_t len = OSCHandler::buildQuery(address, _txBuf, sizeof(_txBuf));
    _udp.beginPacket(Config.mixerIP, Config.oscTxPort);
    _udp.write(_txBuf, len);
    _udp.endPacket();
}

void TalkbackEngine::sendInt(const String& address, int32_t value) {
    if (!_udpOpen) return;
    size_t len = OSCHandler::buildIntMsg(address, value, _txBuf, sizeof(_txBuf));
    _udp.beginPacket(Config.mixerIP, Config.oscTxPort);
    _udp.write(_txBuf, len);
    _udp.endPacket();
}

void TalkbackEngine::sendNoArg(const String& address) {
    sendQuery(address);
}

// ── Channel helpers ───────────────────────────────────────────────────────────

uint8_t TalkbackEngine::channelToSoloID(uint8_t chType, uint8_t chNum) {
    switch (chType) {
        case CH_INPUT:  return SOLO_OFFSET_INPUT  + chNum;   // 1-32
        case CH_AUXIN:  return SOLO_OFFSET_AUXIN  + chNum;   // 33-40
        case CH_FXRTN:  return SOLO_OFFSET_FXRTN  + chNum;   // 41-48
        case CH_BUS:    return SOLO_OFFSET_BUS     + chNum;   // 49-64
        case CH_MATRIX: return SOLO_OFFSET_MATRIX  + chNum;   // 65-70
        case CH_MAIN:   return SOLO_ID_MAIN_LR;               // 71
        case CH_MONO:   return SOLO_ID_MAIN_MONO;             // 72
        case CH_DCA:    return SOLO_OFFSET_DCA     + chNum;   // 73-80
        default:        return chNum;
    }
}

String TalkbackEngine::buildMutePath(uint8_t chType, uint8_t chNum) {
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

void TalkbackEngine::executeActions(const char* jsonStr) {
    if (!jsonStr || jsonStr[0] == '\0') return;

    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, jsonStr);
    if (err) {
        Serial.printf("[TB] Action JSON parse error: %s\n", err.c_str());
        return;
    }

    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject act : arr) {
        const char* t = act["t"] | "";

        if (strcmp(t, "clearSolo") == 0) {
            sendNoArg(TB_CLEARSOLO_PATH);
            Serial.println("[TB]   -> clearSolo");

        } else if (strcmp(t, "solo") == 0) {
            uint8_t id = channelToSoloID(act["ct"] | (uint8_t)0,
                                          act["cn"] | (uint8_t)1);
            sendInt(String(TB_SOLOSW_BASE) + id, 1);
            Serial.printf("[TB]   -> solo  id=%u\n", id);

        } else if (strcmp(t, "unsolo") == 0) {
            uint8_t id = channelToSoloID(act["ct"] | (uint8_t)0,
                                          act["cn"] | (uint8_t)1);
            sendInt(String(TB_SOLOSW_BASE) + id, 0);
            Serial.printf("[TB]   -> unsolo  id=%u\n", id);

        } else if (strcmp(t, "mute") == 0) {
            String path = buildMutePath(act["ct"] | (uint8_t)0,
                                         act["cn"] | (uint8_t)1);
            if (path.length()) sendInt(path, 0);   // 0 = muted on X32/M32
            Serial.printf("[TB]   -> mute  %s\n", path.c_str());

        } else if (strcmp(t, "unmute") == 0) {
            String path = buildMutePath(act["ct"] | (uint8_t)0,
                                         act["cn"] | (uint8_t)1);
            if (path.length()) sendInt(path, 1);   // 1 = active/unmuted
            Serial.printf("[TB]   -> unmute  %s\n", path.c_str());

        } else if (strcmp(t, "osc") == 0) {
            const char* p = act["p"] | "";
            int32_t v     = act["v"] | (int32_t)0;
            if (p[0] != '\0') {
                sendInt(String(p), v);
                Serial.printf("[TB]   -> osc  %s = %d\n", p, v);
            }

        } else if (strcmp(t, "out") == 0) {
            _outputActive = act["s"] | false;
            Serial.printf("[TB]   -> output forced %s\n",
                          _outputActive ? "ON" : "OFF");
        }
    }
}

// ── State-change handlers ─────────────────────────────────────────────────────

void TalkbackEngine::onTalkbackOn(bool isA) {
    Serial.printf("[TB] TALKBACK %s  ON\n", isA ? "A" : "B");
    executeActions(isA ? Config.tbAOnJson : Config.tbBOnJson);
}

void TalkbackEngine::onTalkbackOff(bool isA) {
    Serial.printf("[TB] TALKBACK %s  OFF\n", isA ? "A" : "B");
    executeActions(isA ? Config.tbAOffJson : Config.tbBOffJson);
    // Clear output override once both buttons are released
    if (!_stateA && !_stateB) {
        _outputActive = false;
    }
}

// ── Incoming UDP parser ───────────────────────────────────────────────────────

void TalkbackEngine::processIncoming() {
    int sz = _udp.parsePacket();
    if (sz <= 0) return;

    int rd = _udp.read(_rxBuf, sizeof(_rxBuf) - 1);
    if (rd <= 0) return;

    OSCMessage msg = OSCHandler::parse(_rxBuf, (size_t)rd);
    if (!msg.valid) return;

    // Talkback A
    if (msg.address == TB_PATH_A &&
        (Config.tbMonitor == TB_MONITOR_A || Config.tbMonitor == TB_MONITOR_BOTH)) {
        bool active = (msg.typeTag == 'i') ? (msg.intVal != 0)
                                           : (msg.floatVal >= 0.5f);
        if (active != _stateA) {
            _stateA = active;
            if (_stateA) onTalkbackOn(true); else onTalkbackOff(true);
        }
    }

    // Talkback B
    if (msg.address == TB_PATH_B &&
        (Config.tbMonitor == TB_MONITOR_B || Config.tbMonitor == TB_MONITOR_BOTH)) {
        bool active = (msg.typeTag == 'i') ? (msg.intVal != 0)
                                           : (msg.floatVal >= 0.5f);
        if (active != _stateB) {
            _stateB = active;
            if (_stateB) onTalkbackOn(false); else onTalkbackOff(false);
        }
    }
}

// ── Main loop ─────────────────────────────────────────────────────────────────

void TalkbackEngine::loop() {
    if (!Config.tbEnabled || !_udpOpen) return;

    uint32_t now = millis();

    // Renew /xremote so the X32/M32 pushes changes to us
    if (now - _lastXRemoteMs >= XREMOTE_INTERVAL_MS) {
        sendNoArg("/xremote");
        _lastXRemoteMs = now;
    }

    // Poll talkback state
    if (now - _lastPollMs >= TB_POLL_INTERVAL_MS) {
        if (Config.tbMonitor == TB_MONITOR_A || Config.tbMonitor == TB_MONITOR_BOTH)
            sendQuery(TB_PATH_A);
        if (Config.tbMonitor == TB_MONITOR_B || Config.tbMonitor == TB_MONITOR_BOTH)
            sendQuery(TB_PATH_B);
        _lastPollMs = now;
    }

    processIncoming();
}
