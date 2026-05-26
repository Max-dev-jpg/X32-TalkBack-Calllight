#pragma once
// =============================================================================
// TriggerManager.h  –  Up to MAX_TRIGGERS independent trigger state machines
//
// Each trigger has its own:
//   - EMA smoothing of the raw mixer signal
//   - Threshold + hysteresis + optional inversion
//   - Debounce, hold time, release delay
//   - On/Off action lists (executed via ActionEngine)
// =============================================================================

#include <Arduino.h>
#include "config.h"

class TriggerManager {
public:
    static TriggerManager& instance() {
        static TriggerManager inst;
        return inst;
    }

    void begin();
    void loop();   // processes all triggers; pulls levels from MixerConnection

    // ── Per-trigger queries ───────────────────────────────────────────────────
    bool  isTriggered(uint8_t n) const {
        return (n < MAX_TRIGGERS) ? _state[n].triggered : false;
    }
    float getSmoothedLevel(uint8_t n) const {
        return (n < MAX_TRIGGERS) ? _state[n].smoothed : 0.0f;
    }

    // Returns true if any enabled trigger is currently triggered
    bool isAnyTriggered() const;

    // Backward-compat: trigger-0 state (used by WebServerManager OSC monitor)
    bool  isTriggered()      const { return isTriggered(0); }
    float getSmoothedLevel() const { return getSmoothedLevel(0); }

private:
    TriggerManager() {}

    struct TState {
        float    smoothed       = 0.0f;
        bool     triggered      = false;
        bool     rawAbove       = false;
        bool     debouncing     = false;
        uint32_t debounceStart  = 0;
        uint32_t holdStart      = 0;
        bool     inHold         = false;
        uint32_t releaseStart   = 0;
        bool     inRelease      = false;
    };

    TState _state[MAX_TRIGGERS];

    void processTrigger(uint8_t n, float rawLevel, uint32_t now);
};
