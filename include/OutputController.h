#pragma once
// =============================================================================
// OutputController.h  –  GPIO output with configurable flash modes
// =============================================================================

#include <Arduino.h>

class OutputController {
public:
    // Singleton accessor.
    static OutputController& instance() {
        static OutputController inst;
        return inst;
    }

    void begin();   // configure the output GPIO from current config
    void loop();    // advance the active flash mode and drive the pin

    // Drive output based on trigger state (called by main loop after TriggerLogic)
    void setTrigger(bool active);

    // Pulse the output once for a given duration (test / manual trigger)
    void testPulse(uint32_t durationMs = 2000);

    bool isActive() const { return _active; }

private:
    OutputController() {}

    void setPinState(bool on);   // write the pin, honouring the invert-polarity config

    bool     _active         = false;
    bool     _testMode       = false;
    uint32_t _testEndMs      = 0;

    // Flash timing
    bool     _pinState       = false;
    uint32_t _lastFlipMs     = 0;
    float    _pulsePhase     = 0.0f;   // 0.0-1.0, for PULSE mode
};
