// =============================================================================
// TriggerManager.cpp  –  Multi-trigger state machines
// =============================================================================

#include "TriggerManager.h"
#include "ActionEngine.h"
#include "ConfigManager.h"
#include "MixerConnection.h"
#include "config.h"
#include <Arduino.h>

void TriggerManager::begin() {
    for (uint8_t n = 0; n < MAX_TRIGGERS; n++) {
        _state[n] = TState{};
        ActionEngine::clearOutput(n); // ACT_SRC_TRIGGER_0..3 = 0..3
    }
    Serial.printf("[Trigger] TriggerManager initialised (%u slots)\n", MAX_TRIGGERS);
}

bool TriggerManager::isAnyTriggered() const {
    for (uint8_t n = 0; n < MAX_TRIGGERS; n++) {
        if (Config.triggers[n].enabled && _state[n].triggered) return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Per-trigger state machine
// ─────────────────────────────────────────────────────────────────────────────

void TriggerManager::processTrigger(uint8_t n, float rawLevel, uint32_t now) {
    const TriggerConfig& c = Config.triggers[n];
    TState&              s = _state[n];

    // ── EMA smoothing ─────────────────────────────────────────────────────────
    float alpha = constrain(c.smoothing, 0.01f, 1.0f);
    s.smoothed  = alpha * rawLevel + (1.0f - alpha) * s.smoothed;

    // ── Effective threshold (with hysteresis) ─────────────────────────────────
    float effThresh = s.triggered
                      ? constrain(c.threshold - c.hysteresis, 0.0f, 1.0f)
                      : c.threshold;

    // ── Inversion: "above" means signal meets the activation condition ─────────
    bool above = c.invert ? (s.smoothed < effThresh)
                           : (s.smoothed >= effThresh);

    // ── Debounce ──────────────────────────────────────────────────────────────
    if (above != s.rawAbove) {
        if (!s.debouncing) {
            s.debouncing    = true;
            s.debounceStart = now;
        }
        if (now - s.debounceStart >= c.debounceMs) {
            s.rawAbove    = above;
            s.debouncing  = false;
        }
    } else {
        s.debouncing = false;
    }

    // ── State machine ─────────────────────────────────────────────────────────
    bool prevTriggered = s.triggered;

    if (!s.triggered) {
        if (s.rawAbove) {
            s.triggered  = true;
            s.inHold     = true;
            s.holdStart  = now;
            s.inRelease  = false;
            Serial.printf("[Trigger %u] ACTIVATED  level=%.3f\n", n, s.smoothed);
        }
    } else {
        if (!s.rawAbove) {
            if (s.inHold) {
                // Minimum hold time elapsed? Proceed to release phase
                if (now - s.holdStart >= c.holdTimeMs) {
                    s.inHold      = false;
                    s.inRelease   = true;
                    s.releaseStart = now;
                }
            } else if (s.inRelease) {
                // Release delay elapsed? Deactivate trigger
                if (now - s.releaseStart >= c.releaseDelayMs) {
                    s.triggered = false;
                    s.inRelease = false;
                    Serial.printf("[Trigger %u] RELEASED\n", n);
                }
            }
        } else {
            // Signal re-asserted. Enforce min hold time from initial trigger            
            if (s.inRelease) {
                s.inRelease = false;
                s.inHold = true; 
            }
        }
    }

    // ── Edge actions ──────────────────────────────────────────────────────────
    // ACT_SRC_TRIGGER_0 = 0 … ACT_SRC_TRIGGER_3 = 3
    if (s.triggered && !prevTriggered) {
        ActionEngine::execute(c.onJson, n);
    }
    if (!s.triggered && prevTriggered) {
        ActionEngine::execute(c.offJson, n);
        ActionEngine::clearOutput(n);
    }
}

// ─────────────────────────────────────────────────────────────────────────────

void TriggerManager::loop() {
    uint32_t now = millis();
    for (uint8_t n = 0; n < MAX_TRIGGERS; n++) {
        if (!Config.triggers[n].enabled) {
            // Keep state clean when trigger is disabled
            if (_state[n].triggered) {
                _state[n].triggered = false;
                ActionEngine::clearOutput(n);
            }
            _state[n].smoothed = 0.0f;
            continue;
        }
        float level = MixerConnection::instance().getLevelForTrigger(n);
        processTrigger(n, level, now);
    }
}
