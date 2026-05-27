// =============================================================================
// main.cpp  –  TalkBack CallLight
// ESP32 call light controller for Behringer X32 / Midas M32 mixers
// =============================================================================

#include <Arduino.h>
#include "config.h"
#include "ConfigManager.h"
#include "StorageManager.h"
#include "NetworkManager.h"
#include "MixerConnection.h"
#include "TriggerManager.h"
#include "OutputController.h"
#include "LEDController.h"
#include "WebServerManager.h"
#include "TalkbackEngine.h"
#include "ActionEngine.h"
#include "OSCReceiver.h"

// ─────────────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(200);

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

    // ── 5. Multi-trigger state machines ───────────────────────────────────────
    TriggerManager::instance().begin();

    // ── 6. GPIO output ────────────────────────────────────────────────────────
    OutputController::instance().begin();

    // ── 7. LED strip ──────────────────────────────────────────────────────────
    LEDController::instance().begin();

    // ── 8. Talkback Engine ────────────────────────────────────────────────────
    TalkbackEngine::instance().begin();

    // ── 9. Action Engine (shared executor + output bitmask) ───────────────────
    ActionEngine::begin();

    // ── 10. External OSC Receiver ─────────────────────────────────────────────
    OSCReceiver::instance().begin();

    Serial.println("\n[Main] All systems initialised. Entering main loop.\n");
}

// ─────────────────────────────────────────────────────────────────────────────

void loop() {
    // Network (WiFi reconnect, OTA, mDNS)
    NetworkManager::instance().loop();

    // Mixer: poll OSC, parse responses, update per-trigger levels
    MixerConnection::instance().loop();

    // Run all trigger state machines (pulls levels from MixerConnection internally)
    TriggerManager::instance().loop();

    // Combine all output sources
    bool triggered = TriggerManager::instance().isAnyTriggered()
                  || ActionEngine::isOutputActive()
                  || OSCReceiver::instance().isExtTriggerActive();

    if (Config.outputType == OUTPUT_GPIO || Config.outputType == OUTPUT_BOTH) {
        OutputController::instance().setTrigger(triggered);
        OutputController::instance().loop();
    }

    if (Config.outputType == OUTPUT_WS2812 || Config.outputType == OUTPUT_BOTH) {
        LEDController::instance().setTrigger(triggered);

        // Per-trigger color override: first active trigger with useTriggerColor
        // wins; falls back to global Config.ledR/G/B (set inside setTrigger()).
        for (uint8_t n = 0; n < MAX_TRIGGERS; n++) {
            const TriggerConfig& tc = Config.triggers[n];
            if (tc.enabled && tc.useTriggerColor &&
                TriggerManager::instance().isTriggered(n)) {
                LEDController::instance().setActiveColor(tc.trigLedR,
                                                         tc.trigLedG,
                                                         tc.trigLedB);
                break;
            }
        }

        LEDController::instance().loop();
    }

    // Talkback Engine
    TalkbackEngine::instance().loop();

    // External OSC Receiver
    OSCReceiver::instance().loop();

    // Web server: WS broadcasts, client cleanup
    WebServerManager::instance().loop();
}
