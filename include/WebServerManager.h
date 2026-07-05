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
    // Singleton accessor.
    static WebServerManager& instance() {
        static WebServerManager inst;
        return inst;
    }

    // Mount LittleFS, register routes/WebSocket, and start the HTTP server.
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
    void setupStaticFiles();   // serve the LittleFS web UI (with .gz support)
    void setupAPI();           // register the /api/* REST + WebSocket routes
    void setupWebSocket();     // attach the /ws WebSocket handler

    // ── REST handlers ────────────────────────────────────────────────────────
    void handleGetStatus (AsyncWebServerRequest* req);   // GET /api/status → live JSON
    void handleGetConfig (AsyncWebServerRequest* req);   // GET /api/config → full config JSON
    void handleExportConfig(AsyncWebServerRequest* req); // GET /api/config/export → download
    void handlePostConfig(AsyncWebServerRequest* req,    // POST /api/config → apply + save
                          uint8_t* data, size_t len,
                          size_t index, size_t total);
    void handleReboot        (AsyncWebServerRequest* req);   // POST /api/reboot
    void handleReset         (AsyncWebServerRequest* req);   // POST /api/reset → factory defaults
    void handleTestOutput    (AsyncWebServerRequest* req);   // POST /api/test → pulse outputs
    void handleReconnect     (AsyncWebServerRequest* req);   // POST /api/reconnect → mixer relink
    void handleMonitorToggle (AsyncWebServerRequest* req);   // POST /api/monitor → toggle OSC monitor

    // Send a message only to WS clients whose TX queue has room (weak-link safe)
    void sendToReadyClients(const String& msg);

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
