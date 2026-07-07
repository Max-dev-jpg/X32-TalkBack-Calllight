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
    // Singleton accessor.
    static TriggerManager& instance() {
        static TriggerManager inst;
        return inst;
    }

    void begin();  // reset every trigger's state machine to its at-rest level
    void loop();   // processes all triggers; pulls levels from MixerConnection

    // ── Per-trigger queries ───────────────────────────────────────────────────
    bool  isTriggered(uint8_t n) const {
        return (n < MAX_TRIGGERS) ? _state[n].triggered : false;
    }
    float getSmoothedLevel(uint8_t n) const {
        return (n < MAX_TRIGGERS) ? _state[n].smoothed : 0.0f;
    }
    // Activation timestamp (millis()) of the most recent ON edge; 0 if never triggered.
    // Used by main.cpp for PRIO_NEWEST LED color selection.
    uint32_t getTriggerStartMs(uint8_t n) const {
        return (n < MAX_TRIGGERS) ? _state[n].startMs : 0;
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
        uint32_t lastSmoothMs   = 0;   // last time the EMA was advanced (fixed-rate smoothing)
        bool     triggered      = false;
        bool     rawAbove       = false;
        bool     debouncing     = false;
        uint32_t debounceStart  = 0;
        uint32_t holdStart      = 0;
        bool     inHold         = false;
        uint32_t releaseStart   = 0;
        bool     inRelease      = false;
        uint32_t startMs        = 0;   // millis() of most recent ON edge (for PRIO_NEWEST)
    };

    TState _state[MAX_TRIGGERS];

    // Advance one trigger's state machine: smooth, threshold, debounce/hold/release.
    void processTrigger(uint8_t n, float rawLevel, uint32_t now);
};
