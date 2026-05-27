#pragma once
// =============================================================================
// WebServerManager.h  –  Async web server: static files + REST API + WebSocket
// =============================================================================

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

class WebServerManager {
public:
    static WebServerManager& instance() {
        static WebServerManager inst;
        return inst;
    }

    void begin();

    // Broadcast current status to all WebSocket clients (call every ~1 s)
    void broadcastStatus();

    // Broadcast a single OSC monitor sample (fast, ~200 ms when active)
    void broadcastOSCMonitor();

    // Must be called in loop() to clean up disconnected WS clients
    void loop();

    bool isMonitorActive() const { return _monitorActive; }

private:
    WebServerManager() : _server(80), _ws("/ws") {}

    // ── Route setup ──────────────────────────────────────────────────────────
    void setupStaticFiles();
    void setupAPI();
    void setupWebSocket();

    // ── REST handlers ────────────────────────────────────────────────────────
    void handleGetStatus (AsyncWebServerRequest* req);
    void handleGetConfig (AsyncWebServerRequest* req);
    void handlePostConfig(AsyncWebServerRequest* req,
                          uint8_t* data, size_t len,
                          size_t index, size_t total);
    void handleReboot        (AsyncWebServerRequest* req);
    void handleReset         (AsyncWebServerRequest* req);
    void handleTestOutput    (AsyncWebServerRequest* req);
    void handleReconnect     (AsyncWebServerRequest* req);
    void handleMonitorToggle (AsyncWebServerRequest* req);

    // ── WebSocket event ───────────────────────────────────────────────────────
    static void onWSEvent(AsyncWebSocket* server,
                          AsyncWebSocketClient* client,
                          AwsEventType type,
                          void* arg, uint8_t* data, size_t len);

    AsyncWebServer _server;
    AsyncWebSocket _ws;

    uint32_t _lastBroadcastMs  = 0;
    uint32_t _lastMonitorMs    = 0;
    bool     _monitorActive    = false;

    // Accumulator for chunked POST body
    String   _postBody;
    size_t   _postExpected = 0;
    bool     _postSaveOk   = true;   // result of last applyConfigJSON + save
};
