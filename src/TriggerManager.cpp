// =============================================================================
// TriggerManager.cpp  –  Multi-trigger state machines
// =============================================================================

#include "TriggerManager.h"
#include "ActionEngine.h"
#include "ConfigManager.h"
#include "MixerConnection.h"
#include "MeterScale.h"
#include "config.h"
#include <Arduino.h>

// At-rest level (silence / fader-down / no reduction) for trigger n, in the same
// dB domain the level flows in. Meter/fader levels sit at DB_FLOOR, gate/comp GR
// at 0, mute at 0 — so a freshly reset trigger reads as inactive, not "loud"
// (0 dB = full scale).
static float restLevelFor(uint8_t n) {
    const TriggerConfig& c = Config.triggers[n];
    return MeterScale::restLevel(c.signalSource, c.meterSignalType);
}

// Reset every trigger to its at-rest level and clear its output override.
void TriggerManager::begin() {
    for (uint8_t n = 0; n < MAX_TRIGGERS; n++) {
        _state[n] = TState{};
        _state[n].smoothed = restLevelFor(n);
        ActionEngine::clearOutput(n); // ACT_SRC_TRIGGER_0..3 = 0..3
    }
    DBG_PRINTF("[Trigger] TriggerManager initialised (%u slots)\n", MAX_TRIGGERS);
}

// True if any enabled trigger is currently in the triggered state.
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
    // Mute is binary (0/1) and ignores the dB threshold; everything else compares
    // in dB, so no 0..1 clamp here.
    float effThresh;
    if (c.signalSource == SIG_MUTE) {
        effThresh = 0.5f;
    } else {
        effThresh = s.triggered ? (c.threshold - c.hysteresis) : c.threshold;
    }

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
            DBG_PRINTF("[Trigger %u] ACTIVATED  level=%.3f\n", n, s.smoothed);
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
                    DBG_PRINTF("[Trigger %u] RELEASED\n", n);
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
        s.startMs = now;   // record activation time for PRIO_NEWEST
        ActionEngine::execute(c.onJson, n);
    }
    if (!s.triggered && prevTriggered) {
        ActionEngine::execute(c.offJson, n);
        ActionEngine::clearOutput(n);
    }
}

// ─────────────────────────────────────────────────────────────────────────────

// Run every enabled trigger each iteration; hold disabled/disconnected triggers
// at rest so a stale level can't keep them active.
void TriggerManager::loop() {
    uint32_t now = millis();
    bool mixerConnected = MixerConnection::instance().isConnected();

    for (uint8_t n = 0; n < MAX_TRIGGERS; n++) {
        if (!Config.triggers[n].enabled) {
            // Keep state clean when trigger is disabled
            if (_state[n].triggered) {
                _state[n].triggered = false;
                ActionEngine::clearOutput(n);
            }
            _state[n].smoothed = restLevelFor(n);
            continue;
        }

        if (!mixerConnected) {
            // Do not allow mixer triggers to stay active when the mixer is disconnected.
            if (_state[n].triggered) {
                _state[n].triggered = false;
                ActionEngine::execute(Config.triggers[n].offJson, n);
                ActionEngine::clearOutput(n);
            }
            _state[n].smoothed      = restLevelFor(n);
            _state[n].rawAbove      = false;
            _state[n].debouncing    = false;
            _state[n].inHold        = false;
            _state[n].inRelease     = false;
            _state[n].debounceStart = 0;
            _state[n].holdStart     = 0;
            _state[n].releaseStart  = 0;
            continue;
        }

        float level = MixerConnection::instance().getLevelForTrigger(n);
        processTrigger(n, level, now);
    }
}
