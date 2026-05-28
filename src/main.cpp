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
    DBG_BEGIN(SERIAL_BAUD);
    delay(200);

    DBG_PRINTLN("\n========================================");
    Serial.printf ("  TalkBack CallLight  v%s\n", FIRMWARE_VERSION);
    Serial.printf ("  ESP32 chip: %s  cores: %d\n",
                   ESP.getChipModel(), ESP.getChipCores());
    Serial.printf ("  Free heap: %u bytes\n", ESP.getFreeHeap());
    DBG_PRINTLN("========================================\n");

    // ── 1. Configuration ──────────────────────────────────────────────────────
    ConfigManager::instance().begin();

    // ── 2. LED strip ─────────────────────────────────────────────────────────
    // Turn the strip off immediately after boot, before network or mixer startup.
    LEDController::instance().begin();

    // ── 3. Network ────────────────────────────────────────────────────────────
    NetworkManager::instance().begin();

    // ── 4. Web server (LittleFS + REST + WebSocket) ───────────────────────────
    WebServerManager::instance().begin();

    // ── 5. Mixer OSC connection ───────────────────────────────────────────────
    MixerConnection::instance().begin();

    // ── 6. Multi-trigger state machines ───────────────────────────────────────
    TriggerManager::instance().begin();

    // ── 7. GPIO output ────────────────────────────────────────────────────────
    OutputController::instance().begin();

    // ── 8. Talkback Engine ────────────────────────────────────────────────────
    TalkbackEngine::instance().begin();

    // ── 9. Action Engine (shared executor + output bitmask) ───────────────────
    ActionEngine::begin();

    // ── 10. External OSC Receiver ─────────────────────────────────────────────
    OSCReceiver::instance().begin();

    DBG_PRINTLN("\n[Main] All systems initialised. Entering main loop.\n");
}

// ─────────────────────────────────────────────────────────────────────────────

void loop() {
    // Network (WiFi reconnect, OTA, mDNS)
    NetworkManager::instance().loop();

    // Mixer: poll OSC, parse responses, update per-trigger levels
    MixerConnection::instance().loop();

    // Run all trigger state machines (pulls levels from MixerConnection internally)
    TriggerManager::instance().loop();

    // Combine all output sources.
    // isForcedOff() is set by a 'forceout' action and overrides everything —
    // talkback can silence the call light even while triggers are active.
    bool suppressed = ActionEngine::isForcedOff();
    bool mixerConnected = MixerConnection::instance().isConnected();
    bool triggered  = !suppressed && (
        (mixerConnected && TriggerManager::instance().isAnyTriggered())
        || ActionEngine::isOutputActive()
        || (mixerConnected && OSCReceiver::instance().isExtTriggerActive())
    );

    if (Config.outputType == OUTPUT_GPIO || Config.outputType == OUTPUT_BOTH) {
        OutputController::instance().setTrigger(triggered);
        OutputController::instance().loop();
    }

    if (Config.outputType == OUTPUT_WS2812 || Config.outputType == OUTPUT_BOTH) {

        if (!mixerConnected) {
            // Show a continuously lit dark red status indicator while the mixer is disconnected.
            LEDController::instance().setForceSolidColor(true, 60, 0, 0);
        } else {
            LEDController::instance().setForceSolidColor(false);
            LEDController::instance().setTrigger(triggered);

            // Per-trigger color override with configurable priority
            if (Config.trigPriorityMode == PRIO_NEWEST) {
                // Most recently activated trigger with useTriggerColor wins
                uint32_t latestMs = 0;
                int8_t   winner   = -1;
                for (uint8_t n = 0; n < MAX_TRIGGERS; n++) {
                    const TriggerConfig& tc = Config.triggers[n];
                    if (tc.enabled && tc.useTriggerColor &&
                            TriggerManager::instance().isTriggered(n)) {
                        uint32_t ts = TriggerManager::instance().getTriggerStartMs(n);
                        if (winner < 0 || ts > latestMs) { latestMs = ts; winner = (int8_t)n; }
                    }
                }
                if (winner >= 0) {
                    const TriggerConfig& wc = Config.triggers[(uint8_t)winner];
                    LEDController::instance().setActiveColor(wc.trigLedR,
                                                              wc.trigLedG,
                                                              wc.trigLedB);
                }
            } else {
                // PRIO_FIXED: use trigPriorityOrder[] — first match in list wins
                for (uint8_t i = 0; i < MAX_TRIGGERS; i++) {
                    uint8_t n = Config.trigPriorityOrder[i];
                    if (n >= MAX_TRIGGERS) continue;
                    const TriggerConfig& tc = Config.triggers[n];
                    if (tc.enabled && tc.useTriggerColor &&
                            TriggerManager::instance().isTriggered(n)) {
                        LEDController::instance().setActiveColor(tc.trigLedR,
                                                                  tc.trigLedG,
                                                                  tc.trigLedB);
                        break;
                    }
                }
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
