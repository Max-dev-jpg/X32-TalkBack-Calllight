// =============================================================================
// main.cpp  –  TalkBack CallLight
// ESP32 call light controller for Behringer X32 / Midas M32 mixers
// =============================================================================

#include <Arduino.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
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

// One-time boot: bring up config, LEDs, network, web server, mixer link, and
// all engines in dependency order.
void setup() {
#if DISABLE_BROWNOUT_DETECTOR
    // Must run first — before WiFi/peripherals draw current (see config.h).
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
#endif
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

    // Boot indicator: run a 3x chase across the strip while Wi-Fi associates in
    // the background (association runs in the Wi-Fi task, so this brief blocking
    // animation doesn't delay it).
    if (Config.outputType == OUTPUT_WS2812 || Config.outputType == OUTPUT_BOTH)
        LEDController::instance().bootAnimation(3);

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

// Main loop: service all subsystems, then combine every output source into the
// final GPIO / LED-strip state each iteration.
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
    // External OSC triggers (and 'out' actions) work independently of the mixer
    // link; only mixer-meter triggers require an active connection.
    bool extTriggered = OSCReceiver::instance().isExtTriggerActive();
    bool triggered  = !suppressed && (
        (mixerConnected && TriggerManager::instance().isAnyTriggered())
        || ActionEngine::isOutputActive()
        || extTriggered
    );

    if (Config.outputType == OUTPUT_GPIO || Config.outputType == OUTPUT_BOTH) {
        OutputController::instance().setTrigger(triggered);
        OutputController::instance().loop();
    }

    if (Config.outputType == OUTPUT_WS2812 || Config.outputType == OUTPUT_BOTH) {

        if (!mixerConnected && !triggered && Config.disconnectIndicator) {
            // Dark red "mixer disconnected" indicator (can be disabled in the UI) —
            // an active external OSC trigger (or 'out' action) always takes over the
            // strip even while offline.
            LEDController::instance().setForceSolidColor(true, 60, 0, 0);
        } else {
            LEDController::instance().setForceSolidColor(false);
            LEDController::instance().setTrigger(triggered);

            // Per-trigger color override with configurable priority. A trigger's
            // color counts when it is active via the mixer state machine (enabled)
            // OR via an external OSC command (which drives triggers regardless of
            // the 'enabled' flag), as long as it opted into a custom color.
            auto colorActive = [](uint8_t n) -> bool {
                const TriggerConfig& tc = Config.triggers[n];
                if (!tc.useTriggerColor) return false;
                bool mixerOn = tc.enabled && TriggerManager::instance().isTriggered(n);
                bool oscOn   = OSCReceiver::instance().isExtTriggerActive(n);
                return mixerOn || oscOn;
            };
            // Most recent ON edge across both sources (for PRIO_NEWEST).
            auto colorStartMs = [](uint8_t n) -> uint32_t {
                uint32_t a = TriggerManager::instance().isTriggered(n)
                             ? TriggerManager::instance().getTriggerStartMs(n) : 0;
                uint32_t b = OSCReceiver::instance().isExtTriggerActive(n)
                             ? OSCReceiver::instance().getTriggerStartMs(n) : 0;
                return a > b ? a : b;
            };

            if (Config.trigPriorityMode == PRIO_NEWEST) {
                // Most recently activated trigger with a custom color wins
                uint32_t latestMs = 0;
                int8_t   winner   = -1;
                for (uint8_t n = 0; n < MAX_TRIGGERS; n++) {
                    if (colorActive(n)) {
                        uint32_t ts = colorStartMs(n);
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
                    if (colorActive(n)) {
                        const TriggerConfig& tc = Config.triggers[n];
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
