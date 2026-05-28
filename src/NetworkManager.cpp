// =============================================================================
// NetworkManager.cpp  –  WiFi AP+STA, mDNS, OTA
// =============================================================================

#include "NetworkManager.h"
#include "ConfigManager.h"
#include "config.h"
#include <Arduino.h>

void NetworkManager::begin() {
    Serial.println("[Network] Initialising...");
    _staConnected = false;

    // Boot strategy: Connect to known Wi-Fi or fall back to Access Point
    if (Config.wifiSSID[0] != '\0') {
        startSTA();
    } else {
        Serial.println("[Network] No credentials found. Starting AP.");
        startAP();
    }

    setupMDNS();
    setupOTA();
    Serial.println("[Network] Ready.");
}

// ── Access Point Mode ─────────────────────────────────────────────────────────────

void NetworkManager::startAP() {
    Serial.println("[Network] Entering AP Fallback mode...");
    
    // Stop active connection attempts to stabilize the radio chip
    WiFi.disconnect(); 
    delay(100);
    WiFi.mode(WIFI_AP_STA);

    // Fallback to default password if config is empty
    const char* pw = Config.apPassword[0] != '\0'
                     ? Config.apPassword : DEFAULT_AP_PASSWORD;

    bool ok = WiFi.softAP(DEFAULT_AP_SSID, pw, AP_CHANNEL, 0, AP_MAX_CONNECTIONS);
    if (ok) {
        Serial.printf("[Network] AP Active (STABLE) – SSID: %s  IP: %s\n",
                      DEFAULT_AP_SSID,
                      WiFi.softAPIP().toString().c_str());
    } else {
        Serial.println("[Network] AP start FAILED!");
    }

    _lastReconnectAttempt = millis();
}

// ── Station Mode ───────────────────────────────────────────────────────────────────

void NetworkManager::startSTA() {
    Serial.printf("[Network] Entering STA mode. Connecting to: %s\n", Config.wifiSSID);
    
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true); // Let the driver handle minor reconnects

    // Configure static IP or dynamic DHCP
    if (!Config.useDHCP) {
        IPAddress ip, gw, sn;
        if (ip.fromString(Config.staticIP) &&
            gw.fromString(Config.staticGateway) &&
            sn.fromString(Config.staticSubnet)) {
            WiFi.config(ip, gw, sn);
        }
    } else {
        // Explicitly clear static IP settings to re-enable DHCP
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    }

    WiFi.begin(Config.wifiSSID, Config.wifiPassword);
    _lastReconnectAttempt = millis();

    // refresh snapshot of applied config for change detection
    strncpy(_lastConfig.ssid, Config.wifiSSID, 33);
    strncpy(_lastConfig.password, Config.wifiPassword, 64);
    _lastConfig.useDHCP = Config.useDHCP;
    strncpy(_lastConfig.staticIP, Config.staticIP, 16);
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
    wl_status_t currentStatus = WiFi.status();
    wifi_mode_t currentMode = WiFi.getMode();

    // ─── STATE 1: PURE STATION MODE ──────────────────────────────────────────
    if (currentMode == WIFI_STA) {
        if (currentStatus == WL_CONNECTED) {
            // Wait for a valid DHCP IP before completing connection state
            if (WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
                if (!_staConnected) {
                    _staConnected = true;
                    Serial.printf("[Network] STA connected  IP: %s  RSSI: %d dBm\n",
                                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
                    MDNS.begin(MDNS_HOSTNAME); 
                }

                // Keep RSSI updated every 5 seconds
                if (millis() - _lastRSSIUpdate > 5000) {
                    _rssi = WiFi.RSSI();
                    _lastRSSIUpdate = millis();
                }
            }
        } 
        else {
            // Handle sudden connection drop
            if (_staConnected) {
                _staConnected = false;
                Serial.println("[Network] STA connection lost!");
                _lastReconnectAttempt = millis(); 
            }

            // Fall back to AP mode if connection cannot be re-established
            if (millis() - _lastReconnectAttempt > WIFI_RECONNECT_STA_INTERVAL_MS) {
                Serial.println("[Network] Connection timed out. Switching to AP Fallback...");
                startAP();
            }
        }
    }
    
    // ─── STATE 2: AP + STA FALLBACK MODE ──────────────────────────────────────
    else if (currentMode == WIFI_AP_STA) {
        if (currentStatus == WL_CONNECTED) {
            // Target network found! Shut down AP and switch back to pure STA
            Serial.println("[Network] Target network recovered! Disabling AP.");
            WiFi.mode(WIFI_STA); 
            _staConnected = false; 
        } 
        else {
            // Slow down scanning if a user is active on the AP to protect WebSockets
            unsigned long scanInterval = (WiFi.softAPgetStationNum() > 0) ? WIFI_SCAN_INTERVAL_AP_WITH_CLIENTS_MS : WIFI_SCAN_INTERVAL_AP_NO_CLIENTS_MS;
            int16_t scanResult = WiFi.scanComplete();
            
            // Trigger a new async background scan after interval timeout
            if (scanResult == WIFI_SCAN_FAILED) { 
                if (Config.wifiSSID[0] != '\0' && (millis() - _lastReconnectAttempt > scanInterval)) {
                    Serial.printf("[Network] AP Active. Starting background scan (Interval: %lu ms)...\n", scanInterval);
                    WiFi.scanNetworks(true, false); 
                    _lastReconnectAttempt = millis();
                }
            } 
            // Evaluate scan results once finished
            else if (scanResult >= 0) {
                bool foundTarget = false;
                for (int i = 0; i < scanResult; ++i) {
                    if (WiFi.SSID(i) == String(Config.wifiSSID)) {
                        foundTarget = true;
                        break;
                    }
                }
                
                if (foundTarget) {
                    Serial.println("[Network] Target SSID detected! Switching to STA mode...");
                    WiFi.scanDelete(); 
                    startSTA();
                } else {
                    Serial.println("[Network] Target SSID not found in scan. Keeping AP stable.");
                    WiFi.scanDelete();
                }
            }
        }
    }

    // Process OTA updates if initialized
    if (_otaInitialised) {
        ArduinoOTA.handle();
    }
}


uint8_t NetworkManager::getAPClientCount() const {
    return WiFi.softAPgetStationNum();
}

// ── Config changes  ─────────────────────────────────────────────────────────────
bool NetworkManager::hasConfigChanged() {
    bool changed = (strcmp(_lastConfig.ssid, Config.wifiSSID) != 0) ||
                   (strcmp(_lastConfig.password, Config.wifiPassword) != 0) ||
                   (_lastConfig.useDHCP != Config.useDHCP) ||
                   (strcmp(_lastConfig.staticIP, Config.staticIP) != 0);
    return changed;
}

// ── apply new configurations  ─────────────────────────────────────────────────────────────
void NetworkManager::applyConfig() {
    if (!hasConfigChanged()) {
        Serial.println("[Network] No changes in network config detected. Skipping reconfiguration.");
        return; 
    }

    Serial.println("[Network] Re-evaluating connection...");
    
    WiFi.disconnect(true);
    delay(100);
    
    if (Config.wifiSSID[0] != '\0') {
        startSTA();
    } else {
        startAP();
    }
}
