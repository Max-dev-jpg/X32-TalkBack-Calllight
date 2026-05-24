// =============================================================================
// main.cpp  –  TalkBack CallLight
// ESP32 call light controller for Behringer X32 / Midas M32 mixers
//
// Boot sequence:
//   1. Serial + config init
//   2. Network (AP always on, optional STA)
//   3. LittleFS + web server
//   4. Mixer UDP connection
//   5. Output / LED init
//   6. Main non-blocking loop
// =============================================================================

#include <Arduino.h>
#include "config.h"
#include "ConfigManager.h"
#include "StorageManager.h"
#include "NetworkManager.h"
#include "MixerConnection.h"
#include "TriggerLogic.h"
#include "OutputController.h"
#include "LEDController.h"
#include "WebServerManager.h"
#include "TalkbackEngine.h"

// ─────────────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(200);  // let the serial monitor connect

    Serial.println("\n========================================");
    Serial.printf ("  TalkBack CallLight  v%s\n", FIRMWARE_VERSION);
    Serial.printf ("  ESP32 chip: %s  cores: %d\n",
                   ESP.getChipModel(), ESP.getChipCores());
    Serial.printf ("  Free heap: %u bytes\n", ESP.getFreeHeap());
    Serial.println("========================================\n");

    // ── 1. Configuration ──────────────────────────────────────────────────────
    ConfigManager::instance().begin();

    // ── 2. Network ────────────────────────────────────────────────────────────
    NetworkManager::instance().begin();

    // ── 3. Web server (LittleFS + REST + WebSocket) ───────────────────────────
    WebServerManager::instance().begin();

    // ── 4. Mixer OSC connection ───────────────────────────────────────────────
    MixerConnection::instance().begin();

    // ── 5. Trigger logic ─────────────────────────────────────────────────────
    TriggerLogic::instance().begin();

    // ── 6. GPIO output ───────────────────────────────────────────────────────
    OutputController::instance().begin();

    // ── 7. LED strip ─────────────────────────────────────────────────────────
    LEDController::instance().begin();

    // ── 8. Talkback Engine ────────────────────────────────────────────────────
    TalkbackEngine::instance().begin();

    Serial.println("\n[Main] All systems initialised. Entering main loop.\n");
}

// ─────────────────────────────────────────────────────────────────────────────

void loop() {
    // ── Network (WiFi reconnect, OTA, mDNS) ──────────────────────────────────
    NetworkManager::instance().loop();

    // ── Mixer: poll OSC, parse responses ─────────────────────────────────────
    MixerConnection::instance().loop();

    // ── Feed latest level into trigger logic ─────────────────────────────────
    TriggerLogic::instance().setRawLevel(
        MixerConnection::instance().getCurrentLevel());
    TriggerLogic::instance().loop();

    // ── Drive outputs based on trigger state ─────────────────────────────────
    bool triggered = TriggerLogic::instance().isTriggered();

    if (Config.outputType == OUTPUT_GPIO || Config.outputType == OUTPUT_BOTH) {
        OutputController::instance().setTrigger(triggered);
        OutputController::instance().loop();
    }

    if (Config.outputType == OUTPUT_WS2812 || Config.outputType == OUTPUT_BOTH) {
        LEDController::instance().setTrigger(triggered);
        LEDController::instance().loop();
    }

    // ── Talkback Engine: poll talkback state, send solo commands ─────────────
    TalkbackEngine::instance().loop();

    // ── Web server: WS broadcasts, client cleanup ─────────────────────────────
    WebServerManager::instance().loop();
}
