// =============================================================================
// WebServerManager.cpp  –  Async web server: LittleFS files + REST + WebSocket
// =============================================================================

#include "WebServerManager.h"
#include "ConfigManager.h"
#include "NetworkManager.h"
#include "MixerConnection.h"
#include "TriggerManager.h"
#include "OutputController.h"
#include "LEDController.h"
#include "TalkbackEngine.h"
#include "OSCReceiver.h"
#include "ActionEngine.h"
#include "config.h"
#include <Arduino.h>
#include <Update.h>

// ─────────────────────────────────────────────────────────────────────────────
// Helpers: JSON serialisation
// ─────────────────────────────────────────────────────────────────────────────

static String buildStatusJSON() {
    DynamicJsonDocument doc(1024);
    auto& nm  = NetworkManager::instance();
    auto& mix = MixerConnection::instance();
    auto& tm  = TriggerManager::instance();

    doc["type"]           = "status";
    doc["mixerConnected"] = mix.isConnected();
    doc["rssi"]           = nm.getRSSI();
    doc["staConnected"]   = nm.isSTAConnected();
    doc["staIP"]          = nm.getSTAIP();
    doc["apIP"]           = nm.getAPIP();
    doc["apClients"]      = nm.getAPClientCount();
    doc["uptime"]         = millis() / 1000;
    doc["freeHeap"]       = ESP.getFreeHeap();
    doc["firmware"]       = FIRMWARE_VERSION;
    doc["tbEnabled"]      = Config.tbEnabled;
    doc["tbA"]            = TalkbackEngine::instance().isTalkbackAActive();
    doc["tbB"]            = TalkbackEngine::instance().isTalkbackBActive();
    doc["extOscEnabled"]  = Config.extOscEnabled;
    doc["extOscPort"]     = Config.extOscPort;
    doc["extTrigger"]     = OSCReceiver::instance().isExtTriggerActive();

    // Per-trigger states
    JsonArray tArr = doc.createNestedArray("triggers");
    for (uint8_t n = 0; n < MAX_TRIGGERS; n++) {
        JsonObject t = tArr.createNestedObject();
        t["triggered"] = tm.isTriggered(n);
        t["level"]     = mix.getLevelForTrigger(n);
        t["smoothed"]  = tm.getSmoothedLevel(n);
        t["enabled"]   = Config.triggers[n].enabled;
    }

    // Backward-compat fields (trigger 0)
    doc["level"]     = mix.getLevelForTrigger(0);
    doc["smoothed"]  = tm.getSmoothedLevel(0);
    doc["triggered"] = tm.isAnyTriggered();

    String out;
    serializeJson(doc, out);
    return out;
}

static String buildConfigJSON() {
    DynamicJsonDocument doc(4096);
    const DeviceConfig& c = Config;

    // Network
    doc["apPassword"]    = c.apPassword;
    doc["wifiSSID"]      = c.wifiSSID;
    doc["wifiPassword"]  = c.wifiPassword;
    doc["useDHCP"]       = c.useDHCP;
    doc["staticIP"]      = c.staticIP;
    doc["staticGateway"] = c.staticGateway;
    doc["staticSubnet"]  = c.staticSubnet;

    // Mixer connection
    doc["mixerIP"]   = c.mixerIP;
    doc["oscTxPort"] = c.oscTxPort;
    doc["oscRxPort"] = c.oscRxPort;

    // Triggers array
    JsonArray tArr = doc.createNestedArray("triggers");
    for (uint8_t n = 0; n < MAX_TRIGGERS; n++) {
        const TriggerConfig& t = c.triggers[n];
        JsonObject to = tArr.createNestedObject();
        to["enabled"]       = t.enabled;
        to["channelType"]   = t.channelType;
        to["channelNumber"] = t.channelNumber;
        to["signalSource"]  = t.signalSource;
        to["meterSignalType"] = t.meterSignalType;
        to["customOSCPath"] = t.customOSCPath;
        to["threshold"]     = t.threshold;
        to["hysteresis"]    = t.hysteresis;
        to["smoothing"]     = t.smoothing;
        to["holdTimeMs"]    = t.holdTimeMs;
        to["releaseDelayMs"]= t.releaseDelayMs;
        to["debounceMs"]    = t.debounceMs;
        to["invert"]           = t.invert;
        to["useTriggerColor"]  = t.useTriggerColor;
        to["trigLedR"]         = t.trigLedR;
        to["trigLedG"]         = t.trigLedG;
        to["trigLedB"]         = t.trigLedB;
        to["onJson"]           = t.onJson;
        to["offJson"]          = t.offJson;
        // Resolved OSC path for UI display
        to["resolvedPath"]  = ConfigManager::instance().buildOSCPathForTrigger(t);
    }

    // OSC monitor: trigger-0 resolved path (backward compat)
    doc["oscPath"] = ConfigManager::instance().buildOSCPathForTrigger(c.triggers[0]);

    // Output
    doc["outputType"]   = c.outputType;
    doc["outputPin"]    = c.outputPin;
    doc["outputInvert"] = c.outputInvert;
    doc["flashMode"]    = c.flashMode;
    doc["flashSpeedMs"] = c.flashSpeedMs;

    // LED
    doc["ledPin"]        = c.ledPin;
    doc["ledCount"]      = c.ledCount;
    doc["ledBrightness"] = c.ledBrightness;
    doc["ledR"]          = c.ledR;
    doc["ledG"]          = c.ledG;
    doc["ledB"]          = c.ledB;

    // Talkback Engine
    doc["tbEnabled"]    = c.tbEnabled;
    doc["tbMonitor"]    = c.tbMonitor;
    doc["tbBFollowsA"]  = c.tbBFollowsA;
    doc["tbAOnJson"]    = c.tbAOnJson;
    doc["tbAOffJson"]   = c.tbAOffJson;
    doc["tbBOnJson"]    = c.tbBOnJson;
    doc["tbBOffJson"]   = c.tbBOffJson;

    // Multi-trigger priority
    doc["trigPriorityMode"] = c.trigPriorityMode;
    {
        JsonArray pArr = doc.createNestedArray("trigPriorityOrder");
        for (uint8_t n = 0; n < MAX_TRIGGERS; n++) pArr.add(c.trigPriorityOrder[n]);
    }

    // External OSC
    doc["extOscEnabled"] = c.extOscEnabled;
    doc["extOscPort"]    = c.extOscPort;

    String out;
    serializeJson(doc, out);
    return out;
}

static bool applyConfigJSON(const String& body) {
    DBG_PRINTF("[Web] applyConfigJSON: body len=%u, heap=%u\n",
                  (unsigned)body.length(), (unsigned)ESP.getFreeHeap());

    DynamicJsonDocument doc(4096);
    // Use c_str() for zero-copy: ArduinoJson stores pointers into body's buffer
    // instead of duplicating strings into the pool (saves several KB for action JSON)
    DeserializationError err = deserializeJson(doc, body.c_str());
    if (err != DeserializationError::Ok) {
        DBG_PRINTF("[Web] JSON parse FAILED: %s\n", err.c_str());
        DBG_PRINTF("[Web] Body preview: %.120s\n", body.c_str());
        return false;
    }

    DeviceConfig& c = Config;

    // Network
    if (doc.containsKey("apPassword"))    strlcpy(c.apPassword,   doc["apPassword"],   sizeof(c.apPassword));
    if (doc.containsKey("wifiSSID"))      strlcpy(c.wifiSSID,     doc["wifiSSID"],     sizeof(c.wifiSSID));
    if (doc.containsKey("wifiPassword"))  strlcpy(c.wifiPassword, doc["wifiPassword"], sizeof(c.wifiPassword));
    if (doc.containsKey("useDHCP"))       c.useDHCP = doc["useDHCP"];
    if (doc.containsKey("staticIP"))      strlcpy(c.staticIP,      doc["staticIP"],      sizeof(c.staticIP));
    if (doc.containsKey("staticGateway")) strlcpy(c.staticGateway, doc["staticGateway"], sizeof(c.staticGateway));
    if (doc.containsKey("staticSubnet"))  strlcpy(c.staticSubnet,  doc["staticSubnet"],  sizeof(c.staticSubnet));

    // Mixer connection
    if (doc.containsKey("mixerIP"))    strlcpy(c.mixerIP, doc["mixerIP"], sizeof(c.mixerIP));
    if (doc.containsKey("oscTxPort"))  c.oscTxPort = doc["oscTxPort"];
    if (doc.containsKey("oscRxPort"))  c.oscRxPort = doc["oscRxPort"];

    // Triggers array
    if (doc.containsKey("triggers")) {
        JsonArray tArr = doc["triggers"].as<JsonArray>();
        uint8_t n = 0;
        for (JsonObject to : tArr) {
            if (n >= MAX_TRIGGERS) break;
            TriggerConfig& t = c.triggers[n];
            if (to.containsKey("enabled"))        t.enabled       = to["enabled"];
            if (to.containsKey("channelType"))     t.channelType   = to["channelType"];
            if (to.containsKey("channelNumber"))   t.channelNumber = to["channelNumber"];
            if (to.containsKey("signalSource"))    t.signalSource  = to["signalSource"];
            if (to.containsKey("meterSignalType"))  t.meterSignalType = to["meterSignalType"];
            if (to.containsKey("customOSCPath"))   strlcpy(t.customOSCPath, to["customOSCPath"], sizeof(t.customOSCPath));
            if (to.containsKey("threshold"))       t.threshold     = to["threshold"];
            if (to.containsKey("hysteresis"))      t.hysteresis    = to["hysteresis"];
            if (to.containsKey("smoothing"))       t.smoothing     = to["smoothing"];
            if (to.containsKey("holdTimeMs"))      t.holdTimeMs    = to["holdTimeMs"];
            if (to.containsKey("releaseDelayMs"))  t.releaseDelayMs= to["releaseDelayMs"];
            if (to.containsKey("debounceMs"))      t.debounceMs    = to["debounceMs"];
            if (to.containsKey("invert"))           t.invert          = to["invert"];
            if (to.containsKey("useTriggerColor")) t.useTriggerColor = to["useTriggerColor"];
            if (to.containsKey("trigLedR"))        t.trigLedR        = to["trigLedR"];
            if (to.containsKey("trigLedG"))        t.trigLedG        = to["trigLedG"];
            if (to.containsKey("trigLedB"))        t.trigLedB        = to["trigLedB"];
            if (to.containsKey("onJson"))          strlcpy(t.onJson,  to["onJson"],  sizeof(t.onJson));
            if (to.containsKey("offJson"))         strlcpy(t.offJson, to["offJson"], sizeof(t.offJson));

            DBG_PRINTF("[Web] T%u applied: en=%d chType=%u chNum=%u sig=%u "
                          "thr=%.3f hyst=%.3f smth=%.3f inv=%d\n",
                          n, t.enabled, t.channelType, t.channelNumber,
                          t.signalSource, t.threshold, t.hysteresis,
                          t.smoothing, (int)t.invert);
            n++;
        }
    }

    // Output
    if (doc.containsKey("outputType"))   c.outputType   = doc["outputType"];
    if (doc.containsKey("outputPin"))    c.outputPin    = doc["outputPin"];
    if (doc.containsKey("outputInvert")) c.outputInvert = doc["outputInvert"];
    if (doc.containsKey("flashMode"))    c.flashMode    = doc["flashMode"];
    if (doc.containsKey("flashSpeedMs")) c.flashSpeedMs = doc["flashSpeedMs"];

    // LED
    if (doc.containsKey("ledPin"))        c.ledPin        = doc["ledPin"];
    if (doc.containsKey("ledCount"))      c.ledCount      = doc["ledCount"];
    if (doc.containsKey("ledBrightness")) c.ledBrightness = doc["ledBrightness"];
    if (doc.containsKey("ledR"))          c.ledR          = doc["ledR"];
    if (doc.containsKey("ledG"))          c.ledG          = doc["ledG"];
    if (doc.containsKey("ledB"))          c.ledB          = doc["ledB"];

    // Talkback Engine
    if (doc.containsKey("tbEnabled"))   c.tbEnabled   = doc["tbEnabled"];
    if (doc.containsKey("tbMonitor"))   c.tbMonitor   = doc["tbMonitor"];
    if (doc.containsKey("tbBFollowsA")) c.tbBFollowsA = doc["tbBFollowsA"];
    if (doc.containsKey("tbAOnJson"))   strlcpy(c.tbAOnJson,  doc["tbAOnJson"],  sizeof(c.tbAOnJson));
    if (doc.containsKey("tbAOffJson"))  strlcpy(c.tbAOffJson, doc["tbAOffJson"], sizeof(c.tbAOffJson));
    if (doc.containsKey("tbBOnJson"))   strlcpy(c.tbBOnJson,  doc["tbBOnJson"],  sizeof(c.tbBOnJson));
    if (doc.containsKey("tbBOffJson"))  strlcpy(c.tbBOffJson, doc["tbBOffJson"], sizeof(c.tbBOffJson));

    // Multi-trigger priority
    if (doc.containsKey("trigPriorityMode"))
        c.trigPriorityMode = doc["trigPriorityMode"];
    if (doc.containsKey("trigPriorityOrder")) {
        JsonArray po = doc["trigPriorityOrder"].as<JsonArray>();
        uint8_t i = 0;
        for (JsonVariant v : po) {
            if (i >= MAX_TRIGGERS) break;
            c.trigPriorityOrder[i++] = v.as<uint8_t>();
        }
    }

    // External OSC
    if (doc.containsKey("extOscEnabled")) c.extOscEnabled = doc["extOscEnabled"];
    if (doc.containsKey("extOscPort"))    c.extOscPort    = doc["extOscPort"];

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// WebSocket event
// ─────────────────────────────────────────────────────────────────────────────

void WebServerManager::onWSEvent(AsyncWebSocket* srv,
                                  AsyncWebSocketClient* client,
                                  AwsEventType type,
                                  void* arg, uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        DBG_PRINTF("[WS] Client #%u connected from %s\n",
                      client->id(),
                      client->remoteIP().toString().c_str());
        client->text(buildStatusJSON());
    } else if (type == WS_EVT_DISCONNECT) {
        DBG_PRINTF("[WS] Client #%u disconnected\n", client->id());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Route setup
// ─────────────────────────────────────────────────────────────────────────────

void WebServerManager::setupStaticFiles() {
    _server.serveStatic("/", LittleFS, "/")
           .setDefaultFile("index.html")
           .setCacheControl("no-cache");
}

void WebServerManager::setupWebSocket() {
    _ws.onEvent(WebServerManager::onWSEvent);
    _server.addHandler(&_ws);
}

void WebServerManager::setupAPI() {
    _server.on("/api/status", HTTP_GET,
        [this](AsyncWebServerRequest* req) { handleGetStatus(req); });

    _server.on("/api/config", HTTP_GET,
        [this](AsyncWebServerRequest* req) { handleGetConfig(req); });

    _server.on("/api/config", HTTP_POST,
        [this](AsyncWebServerRequest* req) {
            // _postSaveOk is set by handlePostConfig (body handler runs first)
            if (_postSaveOk) {
                req->send(200, "application/json", "{\"ok\":true}");
            } else {
                req->send(500, "application/json",
                          "{\"ok\":false,\"message\":\"Config parse or save failed — check serial log\"}");
            }
        },
        nullptr,
        [this](AsyncWebServerRequest* req, uint8_t* data,
               size_t len, size_t index, size_t total) {
            handlePostConfig(req, data, len, index, total);
        });

    _server.on("/api/reboot", HTTP_POST,
        [this](AsyncWebServerRequest* req) { handleReboot(req); });

    _server.on("/api/reset", HTTP_POST,
        [this](AsyncWebServerRequest* req) { handleReset(req); });

    _server.on("/api/test", HTTP_POST,
        [this](AsyncWebServerRequest* req) { handleTestOutput(req); });

    _server.on("/api/reconnect", HTTP_POST,
        [this](AsyncWebServerRequest* req) { handleReconnect(req); });

    _server.on("/api/monitor", HTTP_POST,
        [this](AsyncWebServerRequest* req) { handleMonitorToggle(req); });

    _server.onNotFound([](AsyncWebServerRequest* req) {
        req->send(404, "text/plain", "Not found");
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// Handler implementations
// ─────────────────────────────────────────────────────────────────────────────

void WebServerManager::handleGetStatus(AsyncWebServerRequest* req) {
    req->send(200, "application/json", buildStatusJSON());
}

void WebServerManager::handleGetConfig(AsyncWebServerRequest* req) {
    req->send(200, "application/json", buildConfigJSON());
}

void WebServerManager::handlePostConfig(AsyncWebServerRequest* req,
                                         uint8_t* data, size_t len,
                                         size_t index, size_t total) {
    if (index == 0) {
        _postBody    = "";
        _postExpected = total;
    }
    _postBody += String((char*)data, len);

    if (index + len >= total) {
        _postSaveOk = applyConfigJSON(_postBody);
        if (_postSaveOk) {
            _postSaveOk = ConfigManager::instance().save();
            OutputController::instance().begin();
            LEDController::instance().begin();
            MixerConnection::instance().reconnect();
            TalkbackEngine::instance().begin();
            TriggerManager::instance().begin();
            OSCReceiver::instance().begin();
            NetworkManager::instance().applyConfig();
            DBG_PRINTLN("[Web] Config updated and saved.");
        } else {
            DBG_PRINTLN("[Web] Config apply/save FAILED!");
        }
    }
}

void WebServerManager::handleReboot(AsyncWebServerRequest* req) {
    req->send(200, "application/json", "{\"ok\":true,\"message\":\"Rebooting...\"}");
    delay(500);
    ESP.restart();
}

void WebServerManager::handleReset(AsyncWebServerRequest* req) {
    ConfigManager::instance().resetToDefaults();
    req->send(200, "application/json",
              "{\"ok\":true,\"message\":\"Settings reset. Rebooting...\"}");
    delay(500);
    ESP.restart();
}

void WebServerManager::handleTestOutput(AsyncWebServerRequest* req) {
    OutputController::instance().testPulse(3000);
    LEDController::instance().testPulse(3000);
    req->send(200, "application/json", "{\"ok\":true}");
}

void WebServerManager::handleReconnect(AsyncWebServerRequest* req) {
    MixerConnection::instance().reconnect();
    req->send(200, "application/json", "{\"ok\":true}");
}

void WebServerManager::handleMonitorToggle(AsyncWebServerRequest* req) {
    _monitorActive = !_monitorActive;
    String body = _monitorActive
        ? "{\"ok\":true,\"active\":true}"
        : "{\"ok\":true,\"active\":false}";
    req->send(200, "application/json", body);
}

// ─────────────────────────────────────────────────────────────────────────────
// Public entry points
// ─────────────────────────────────────────────────────────────────────────────

void WebServerManager::begin() {
    if (!LittleFS.begin(true)) {
        DBG_PRINTLN("[Web] LittleFS mount FAILED!");
    } else {
        DBG_PRINTLN("[Web] LittleFS mounted.");
    }

    setupWebSocket();
    setupAPI();
    setupStaticFiles();

    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    _server.begin();
    DBG_PRINTLN("[Web] HTTP server started on port 80.");
}

void WebServerManager::broadcastStatus() {
    if (_ws.count() == 0) return;
    _ws.textAll(buildStatusJSON());
}

void WebServerManager::broadcastOSCMonitor() {
    if (_ws.count() == 0) return;

    // Send all 4 trigger levels in one message
    DynamicJsonDocument doc(512);
    doc["type"] = "osc";
    doc["ts"]   = millis();

    JsonArray arr = doc.createNestedArray("monitors");
    for (uint8_t n = 0; n < MAX_TRIGGERS; n++) {
        const TriggerConfig& tc = Config.triggers[n];
        JsonObject m = arr.createNestedObject();
        m["n"]      = n;
        // Display the resolved source: "/meters/<bank>[idx=N]" for meter mode,
        // or the real OSC path on the mixer for fader/mute. A postfader trigger
        // blocked by the single /meters/6 slot is flagged explicitly.
        if (tc.signalSource == SIG_METER && tc.enabled &&
            MixerConnection::instance().isMeterBlocked(n)) {
            m["address"] = String("/meters/6 BLOCKED (in use)");
        } else {
            m["address"] = ConfigManager::instance().buildOSCPathForTrigger(tc);
        }
        m["value"]     = MixerConnection::instance().getLevelForTrigger(n);
        m["smoothed"]  = TriggerManager::instance().getSmoothedLevel(n);
        m["triggered"] = TriggerManager::instance().isTriggered(n);
        m["enabled"]   = tc.enabled;
    }

    String out;
    serializeJson(doc, out);
    _ws.textAll(out);
}

void WebServerManager::loop() {
    _ws.cleanupClients();

    uint32_t now = millis();

    if (now - _lastBroadcastMs >= WS_BROADCAST_INTERVAL) {
        _lastBroadcastMs = now;
        broadcastStatus();
    }

    if (_monitorActive && now - _lastMonitorMs >= 200) {
        _lastMonitorMs = now;
        broadcastOSCMonitor();
    }
}
