// =============================================================================
// TriggerLogic.cpp  –  Signal processing: EMA smoothing, threshold, hold, release
// =============================================================================

#include "TriggerLogic.h"
#include "ConfigManager.h"
#include "config.h"
#include <Arduino.h>

void TriggerLogic::begin() {
    _smoothed        = 0.0f;
    _triggered       = false;
    _rawAbove        = false;
    _debouncing      = false;
    _inHold          = false;
    _inRelease       = false;
}

// Called by MixerConnection each time a new sample arrives
void TriggerLogic::setRawLevel(float level) {
    // Exponential moving average (EMA) smoothing
    float alpha = constrain(Config.smoothing, 0.01f, 1.0f);
    _smoothed = alpha * level + (1.0f - alpha) * _smoothed;
}

void TriggerLogic::loop() {
    uint32_t now = millis();
    const DeviceConfig& c = Config;

    // ── Effective threshold with hysteresis ───────────────────────────────────
    // When triggered: lower threshold (stays active longer)
    // When not triggered: full threshold (guards against noise)
    float effectiveThresh = _triggered
                            ? (c.threshold - c.hysteresis)
                            : (c.threshold);
    effectiveThresh = constrain(effectiveThresh, 0.0f, 1.0f);

    bool above = (_smoothed >= effectiveThresh);

    // ── Debounce ─────────────────────────────────────────────────────────────
    if (above != _rawAbove) {
        if (!_debouncing) {
            _debouncing     = true;
            _debounceStartMs = now;
        }
        // Wait for debounce period before accepting state change
        if (millis() - _debounceStartMs >= c.debounceMs) {
            _rawAbove   = above;
            _debouncing = false;
        }
    } else {
        _debouncing = false;
    }

    // ── State machine ─────────────────────────────────────────────────────────

    if (!_triggered) {
        // Trigger rising edge
        if (_rawAbove) {
            _triggered   = true;
            _inHold      = true;
            _holdStartMs = now;
            _inRelease   = false;
            Serial.println("[Trigger] ACTIVATED");
        }
    } else {
        // Currently triggered — check if signal dropped below threshold
        if (!_rawAbove) {
            if (_inHold) {
                // Keep active for hold time even if signal drops
                if (now - _holdStartMs >= c.holdTimeMs) {
                    _inHold    = false;
                    // Start release delay
                    _inRelease     = true;
                    _releaseStartMs = now;
                }
            } else if (_inRelease) {
                // Wait out the release delay before de-triggering
                if (now - _releaseStartMs >= c.releaseDelayMs) {
                    _triggered = false;
                    _inRelease = false;
                    Serial.println("[Trigger] RELEASED");
                }
            }
        } else {
            // Signal came back up — reset timers
            _inHold      = true;
            _holdStartMs = now;
            _inRelease   = false;
        }
    }
}
