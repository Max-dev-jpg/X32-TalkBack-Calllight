#pragma once
// =============================================================================
// TriggerLogic.h  –  Signal processing: smoothing, threshold, hold, debounce
// =============================================================================

#include <Arduino.h>

class TriggerLogic {
public:
    static TriggerLogic& instance() {
        static TriggerLogic inst;
            return inst;
    }

    void begin();

    // Feed a raw level sample (called by MixerConnection each poll)
    void setRawLevel(float level);

    // Must be called every loop() iteration to advance state machine
    void loop();

    // ── Outputs ──────────────────────────────────────────────────────────────
    bool  isTriggered()    const { return _triggered; }
    float getSmoothedLevel() const { return _smoothed; }

private:
    TriggerLogic() {}

    float    _smoothed       = 0.0f;
    bool     _triggered      = false;

    // Debounce / hold state
    bool     _rawAbove       = false;   // raw signal above effective threshold
    bool     _debouncing     = false;
    uint32_t _debounceStartMs = 0;

    // Hold timer: keeps output high for at least holdTimeMs after trigger
    uint32_t _holdStartMs     = 0;
    bool     _inHold          = false;

    // Release delay timer
    uint32_t _releaseStartMs  = 0;
    bool     _inRelease       = false;
};
