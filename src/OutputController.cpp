// =============================================================================
// OutputController.cpp  –  GPIO output with non-blocking flash modes
// =============================================================================

#include "OutputController.h"
#include "ConfigManager.h"
#include "config.h"
#include <Arduino.h>
#include <math.h>

void OutputController::begin() {
    uint8_t pin = Config.outputPin;
    pinMode(pin, OUTPUT);
    setPinState(false);
    DBG_PRINTF("[Output] GPIO pin %d initialised.\n", pin);
}

void OutputController::setPinState(bool on) {
    bool physical = Config.outputInvert ? !on : on;
    digitalWrite(Config.outputPin, physical ? HIGH : LOW);
    _pinState = on;
}

void OutputController::setTrigger(bool active) {
    _active = active;
}

void OutputController::testPulse(uint32_t durationMs) {
    _testMode  = true;
    _testEndMs = millis() + durationMs;
    DBG_PRINTF("[Output] Test pulse for %u ms\n", durationMs);
}

void OutputController::loop() {
    uint32_t now = millis();

    // Test mode overrides normal operation
    if (_testMode) {
        if (now < _testEndMs) {
            _active = true;
        } else {
            _testMode = false;
            _active   = false;
        }
    }

    if (!_active) {
        setPinState(false);
        return;
    }

    // Flash mode logic
    uint32_t half = Config.flashSpeedMs / 2;

    switch (Config.flashMode) {
        case FLASH_SOLID:
            setPinState(true);
            break;

        case FLASH_BLINK:
            if (now - _lastFlipMs >= half) {
                _lastFlipMs = now;
                setPinState(!_pinState);
            }
            break;

        case FLASH_STROBE: {
            // Strobe: very short on pulse
            uint32_t period = max((uint32_t)50, (uint32_t)Config.flashSpeedMs);
            uint32_t onTime = period / 8;   // 12.5% duty cycle
            uint32_t t = now % period;
            setPinState(t < onTime);
            break;
        }

        case FLASH_PULSE:
            // Sine-wave brightness: map to on/off via PWM-less threshold trick
            // (true PWM would need ledc; for simplicity use 50% threshold)
            if (now - _lastFlipMs >= half) {
                _lastFlipMs = now;
                setPinState(!_pinState);
            }
            break;

        default:
            setPinState(true);
    }
}
