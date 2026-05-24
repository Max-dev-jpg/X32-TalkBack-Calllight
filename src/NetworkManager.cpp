// =============================================================================
// NetworkManager.cpp  –  WiFi AP+STA, mDNS, OTA
// =============================================================================

#include "NetworkManager.h"
#include "ConfigManager.h"
#include "config.h"
#include <Arduino.h>

void NetworkManager::begin() {
    Serial.println("[Network] Initialising...");

    // AP + STA dual mode
    WiFi.mode(WIFI_AP_STA);
    WiFi.setAutoReconnect(false);   // we handle reconnect ourselves

    startAP();

    if (Config.wifiSSID[0] != '\0') {
        startSTA();
    }

    setupMDNS();
    setupOTA();

    Serial.println("[Network] Ready.");
}

// ── Access Point ─────────────────────────────────────────────────────────────

void NetworkManager::startAP() {
    const char* pw = Config.apPassword[0] != '\0'
                     ? Config.apPassword : DEFAULT_AP_PASSWORD;

    bool ok = WiFi.softAP(DEFAULT_AP_SSID, pw, AP_CHANNEL, 0, AP_MAX_CONNECTIONS);
    if (ok) {
        Serial.printf("[Network] AP  SSID: %s  IP: %s\n",
                      DEFAULT_AP_SSID,
                      WiFi.softAPIP().toString().c_str());
    } else {
        Serial.println("[Network] AP start FAILED!");
    }
}

// ── Station ───────────────────────────────────────────────────────────────────

void NetworkManager::startSTA() {
    if (Config.wifiSSID[0] == '\0') return;

    Serial.printf("[Network] STA connecting to: %s\n", Config.wifiSSID);

    if (!Config.useDHCP) {
        IPAddress ip, gw, sn;
        if (ip.fromString(Config.staticIP) &&
            gw.fromString(Config.staticGateway) &&
            sn.fromString(Config.staticSubnet)) {
            WiFi.config(ip, gw, sn);
        }
    }

    WiFi.begin(Config.wifiSSID, Config.wifiPassword);
    _lastReconnectAttempt = millis();
}

void NetworkManager::checkSTAConnection() {
    bool nowConnected = (WiFi.status() == WL_CONNECTED);

    if (nowConnected != _staConnected) {
        _staConnected = nowConnected;
        if (nowConnected) {
            Serial.printf("[Network] STA connected  IP: %s  RSSI: %d dBm\n",
                          WiFi.localIP().toString().c_str(),
                          WiFi.RSSI());
            // Refresh mDNS on reconnect
            MDNS.begin(MDNS_HOSTNAME);
        } else {
            Serial.println("[Network] STA disconnected.");
        }
    }
}

void NetworkManager::reconnectSTA() {
    WiFi.disconnect();
    delay(100);
    startSTA();
}

// ── mDNS ─────────────────────────────────────────────────────────────────────

void NetworkManager::setupMDNS() {
    if (MDNS.begin(MDNS_HOSTNAME)) {
        MDNS.addService("http", "tcp", WEB_SERVER_PORT);
        Serial.printf("[Network] mDNS: http://%s.local\n", MDNS_HOSTNAME);
    } else {
        Serial.println("[Network] mDNS start failed.");
    }
}

// ── OTA ───────────────────────────────────────────────────────────────────────

void NetworkManager::setupOTA() {
    ArduinoOTA.setHostname(MDNS_HOSTNAME);

    if (strlen(OTA_PASSWORD) > 0) {
        ArduinoOTA.setPassword(OTA_PASSWORD);
    }

    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
        Serial.println("[OTA] Starting update: " + type);
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\n[OTA] Done. Rebooting...");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("[OTA] %u%%\r", progress * 100 / total);
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("[OTA] Error[%u]: ", error);
        const char* msg[] = {"Auth Failed","Begin Failed","Connect Failed",
                             "Receive Failed","End Failed"};
        if (error < 5) Serial.println(msg[error]);
    });

    ArduinoOTA.begin();
    _otaInitialised = true;
    Serial.println("[Network] OTA ready.");
}

// ── Main loop ─────────────────────────────────────────────────────────────────

void NetworkManager::loop() {
    checkSTAConnection();

    // Auto-reconnect STA if we have credentials but lost connection
    if (!_staConnected &&
        Config.wifiSSID[0] != '\0' &&
        millis() - _lastReconnectAttempt > WIFI_RECONNECT_INTERVAL_MS) {
        Serial.println("[Network] Attempting STA reconnect...");
        reconnectSTA();
        _lastReconnectAttempt = millis();
    }

    // Update RSSI periodically
    if (_staConnected && millis() - _lastRSSIUpdate > 5000) {
        _rssi = WiFi.RSSI();
        _lastRSSIUpdate = millis();
    }

    if (_otaInitialised) {
        ArduinoOTA.handle();
    }
}

uint8_t NetworkManager::getAPClientCount() const {
    return WiFi.softAPgetStationNum();
}
