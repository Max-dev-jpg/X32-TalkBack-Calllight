// =============================================================================
// WebServerManager.cpp  –  Async web server: LittleFS files + REST + WebSocket
// =============================================================================

#include "WebServerManager.h"
#include "ConfigManager.h"
#include "NetworkManager.h"
#include "MixerConnection.h"
#include "TriggerLogic.h"
#include "OutputController.h"
#include "LEDController.h"
#include "config.h"
#include <Arduino.h>
#include <Update.h>

// ─────────────────────────────────────────────────────────────────────────────
// Helpers: JSON serialisation
// ─────────────────────────────────────────────────────────────────────────────

static String buildStatusJSON() {
    DynamicJsonDocument doc(512);
    auto& nm  = NetworkManager::instance();
    auto& mix = MixerConnection::instance();
    auto& trig = TriggerLogic::instance();

    doc["mixerConnected"] = mix.isConnected();
    doc["level"]          = mix.getCurrentLevel();
    doc["smoothed"]       = trig.getSmoothedLevel();
    doc["triggered"]      = trig.isTriggered();
    doc["rssi"]           = nm.getRSSI();
    doc["staConnected"]   = nm.isSTAConnected();
    doc["staIP"]          = nm.getSTAIP();
    doc["apIP"]           = nm.getAPIP();
    doc["apClients"]      = nm.getAPClientCount();
    doc["uptime"]         = millis() / 1000;
    doc["freeHeap"]       = ESP.getFreeHeap();
    doc["firmware"]       = FIRMWARE_VERSION;

    String out;
    serializeJson(doc, out);
    return out;
}

static String buildConfigJSON() {
    DynamicJsonDocument doc(2048);
    const DeviceConfig& c = Config;

    // Network
    doc["apPassword"]     = c.apPassword;
    doc["wifiSSID"]       = c.wifiSSID;
    doc["wifiPassword"]   = c.wifiPassword;
    doc["useDHCP"]        = c.useDHCP;
    doc["staticIP"]       = c.staticIP;
    doc["staticGateway"]  = c.staticGateway;
    doc["staticSubnet"]   = c.staticSubnet;

    // Mixer
    doc["mixerIP"]        = c.mixerIP;
    doc["oscTxPort"]      = c.oscTxPort;
    doc["oscRxPort"]      = c.oscRxPort;
    doc["mixerType"]      = c.mixerType;
    doc["channelType"]    = c.channelType;
    doc["channelNumber"]  = c.channelNumber;
    doc["signalSource"]   = c.signalSource;
    doc["customOSCPath"]  = c.customOSCPath;
    doc["oscPath"]        = ConfigManager::instance().buildOSCPath();

    // Trigger
    doc["threshold"]      = c.threshold;
    doc["holdTimeMs"]     = c.holdTimeMs;
    doc["releaseDelayMs"] = c.releaseDelayMs;
    doc["hysteresis"]     = c.hysteresis;
    doc["smoothing"]      = c.smoothing;
    doc["debounceMs"]     = c.debounceMs;

    // Output
    doc["outputType"]     = c.outputType;
    doc["outputPin"]      = c.outputPin;
    doc["outputInvert"]   = c.outputInvert;
    doc["flashMode"]      = c.flashMode;
    doc["flashSpeedMs"]   = c.flashSpeedMs;

    // LED
    doc["ledPin"]         = c.ledPin;
    doc["ledCount"]       = c.ledCount;
    doc["ledBrightness"]  = c.ledBrightness;
    doc["ledR"]           = c.ledR;
    doc["ledG"]           = c.ledG;
    doc["ledB"]           = c.ledB;

    String out;
    serializeJson(doc, out);
    return out;
}

// Apply a JSON config document to the DeviceConfig struct
static bool applyConfigJSON(const String& body) {
    DynamicJsonDocument doc(2048);
    if (deserializeJson(doc, body) != DeserializationError::Ok) return false;

    DeviceConfig& c = Config;

    // Network
    if (doc.containsKey("apPassword"))    strlcpy(c.apPassword,   doc["apPassword"],   sizeof(c.apPassword));
    if (doc.containsKey("wifiSSID"))      strlcpy(c.wifiSSID,     doc["wifiSSID"],     sizeof(c.wifiSSID));
    if (doc.containsKey("wifiPassword"))  strlcpy(c.wifiPassword, doc["wifiPassword"], sizeof(c.wifiPassword));
    if (doc.containsKey("useDHCP"))       c.useDHCP = doc["useDHCP"];
    if (doc.containsKey("staticIP"))      strlcpy(c.staticIP,      doc["staticIP"],      sizeof(c.staticIP));
    if (doc.containsKey("staticGateway")) strlcpy(c.staticGateway, doc["staticGateway"], sizeof(c.staticGateway));
    if (doc.containsKey("staticSubnet"))  strlcpy(c.staticSubnet,  doc["staticSubnet"],  sizeof(c.staticSubnet));

    // Mixer
    if (doc.containsKey("mixerIP"))       strlcpy(c.mixerIP, doc["mixerIP"], sizeof(c.mixerIP));
    if (doc.containsKey("oscTxPort"))     c.oscTxPort     = doc["oscTxPort"];
    if (doc.containsKey("oscRxPort"))     c.oscRxPort     = doc["oscRxPort"];
    if (doc.containsKey("mixerType"))     c.mixerType     = doc["mixerType"];
    if (doc.containsKey("channelType"))   c.channelType   = doc["channelType"];
    if (doc.containsKey("channelNumber")) c.channelNumber = doc["channelNumber"];
    if (doc.containsKey("signalSource"))  c.signalSource  = doc["signalSource"];
    if (doc.containsKey("customOSCPath")) strlcpy(c.customOSCPath, doc["customOSCPath"], sizeof(c.customOSCPath));

    // Trigger
    if (doc.containsKey("threshold"))      c.threshold      = doc["threshold"];
    if (doc.containsKey("holdTimeMs"))     c.holdTimeMs     = doc["holdTimeMs"];
    if (doc.containsKey("releaseDelayMs")) c.releaseDelayMs = doc["releaseDelayMs"];
    if (doc.containsKey("hysteresis"))     c.hysteresis     = doc["hysteresis"];
    if (doc.containsKey("smoothing"))      c.smoothing      = doc["smoothing"];
    if (doc.containsKey("debounceMs"))     c.debounceMs     = doc["debounceMs"];

    // Output
    if (doc.containsKey("outputType"))    c.outputType   = doc["outputType"];
    if (doc.containsKey("outputPin"))     c.outputPin    = doc["outputPin"];
    if (doc.containsKey("outputInvert"))  c.outputInvert = doc["outputInvert"];
    if (doc.containsKey("flashMode"))     c.flashMode    = doc["flashMode"];
    if (doc.containsKey("flashSpeedMs"))  c.flashSpeedMs = doc["flashSpeedMs"];

    // LED
    if (doc.containsKey("ledPin"))        c.ledPin        = doc["ledPin"];
    if (doc.containsKey("ledCount"))      c.ledCount      = doc["ledCount"];
    if (doc.containsKey("ledBrightness")) c.ledBrightness = doc["ledBrightness"];
    if (doc.containsKey("ledR"))          c.ledR          = doc["ledR"];
    if (doc.containsKey("ledG"))          c.ledG          = doc["ledG"];
    if (doc.containsKey("ledB"))          c.ledB          = doc["ledB"];

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
        Serial.printf("[WS] Client #%u connected from %s\n",
                      client->id(),
                      client->remoteIP().toString().c_str());
        // Send current status immediately on connect
        client->text(buildStatusJSON());
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("[WS] Client #%u disconnected\n", client->id());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Route setup
// ─────────────────────────────────────────────────────────────────────────────

void WebServerManager::setupStaticFiles() {
    // Serve everything in LittleFS with caching headers
    _server.serveStatic("/", LittleFS, "/")
           .setDefaultFile("index.html")
           .setCacheControl("max-age=600");
}

void WebServerManager::setupWebSocket() {
    _ws.onEvent(WebServerManager::onWSEvent);
    _server.addHandler(&_ws);
}

void WebServerManager::setupAPI() {
    // ── GET /api/status ───────────────────────────────────────────────────────
    _server.on("/api/status", HTTP_GET,
        [this](AsyncWebServerRequest* req) { handleGetStatus(req); });

    // ── GET /api/config ───────────────────────────────────────────────────────
    _server.on("/api/config", HTTP_GET,
        [this](AsyncWebServerRequest* req) { handleGetConfig(req); });

    // ── POST /api/config ──────────────────────────────────────────────────────
    _server.on("/api/config", HTTP_POST,
        [this](AsyncWebServerRequest* req) {
            // Body processing is handled by the body handler below
            req->send(200, "application/json", "{\"ok\":true}");
        },
        nullptr,  // upload handler
        [this](AsyncWebServerRequest* req, uint8_t* data,
               size_t len, size_t index, size_t total) {
            handlePostConfig(req, data, len, index, total);
        });

    // ── POST /api/reboot ──────────────────────────────────────────────────────
    _server.on("/api/reboot", HTTP_POST,
        [this](AsyncWebServerRequest* req) { handleReboot(req); });

    // ── POST /api/reset ───────────────────────────────────────────────────────
    _server.on("/api/reset", HTTP_POST,
        [this](AsyncWebServerRequest* req) { handleReset(req); });

    // ── POST /api/test ────────────────────────────────────────────────────────
    _server.on("/api/test", HTTP_POST,
        [this](AsyncWebServerRequest* req) { handleTestOutput(req); });

    // ── POST /api/reconnect ───────────────────────────────────────────────────
    _server.on("/api/reconnect", HTTP_POST,
        [this](AsyncWebServerRequest* req) { handleReconnect(req); });

    // ── 404 fallback ──────────────────────────────────────────────────────────
    _server.onNotFound([](AsyncWebServerRequest* req) {
        // For captive portal behaviour: redirect to AP IP
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
    // Accumulate chunked body
    if (index == 0) {
        _postBody    = "";
        _postExpected = total;
    }
    _postBody += String((char*)data).substring(0, len);

    if (index + len >= total) {
        // Full body received
        if (applyConfigJSON(_postBody)) {
            ConfigManager::instance().save();
            // Reinit output controllers with new settings
            OutputController::instance().begin();
            LEDController::instance().begin();
            MixerConnection::instance().reconnect();
            Serial.println("[Web] Config updated.");
        } else {
            Serial.println("[Web] Config parse failed!");
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
    req->send(200, "application/json", "{\"ok\":true,\"message\":\"Settings reset. Rebooting...\"}");
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

// ─────────────────────────────────────────────────────────────────────────────
// Public entry points
// ─────────────────────────────────────────────────────────────────────────────

void WebServerManager::begin() {
    if (!LittleFS.begin(true)) {
        Serial.println("[Web] LittleFS mount FAILED! Web UI will not be available.");
    } else {
        Serial.println("[Web] LittleFS mounted.");
    }

    setupWebSocket();
    setupAPI();
    setupStaticFiles();

    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    _server.begin();
    Serial.println("[Web] HTTP server started on port 80.");
}

void WebServerManager::broadcastStatus() {
    if (_ws.count() == 0) return;
    _ws.textAll(buildStatusJSON());
}

void WebServerManager::loop() {
    _ws.cleanupClients();

    if (millis() - _lastBroadcastMs >= WS_BROADCAST_INTERVAL) {
        _lastBroadcastMs = millis();
        broadcastStatus();
    }
}
