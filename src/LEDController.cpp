// =============================================================================
// LEDController.cpp  –  WS2812B / NeoPixel strip with non-blocking flash modes
// =============================================================================

#include "LEDController.h"
#include "ConfigManager.h"
#include "config.h"
#include <Arduino.h>
#include <math.h>

void LEDController::begin() {
    uint8_t  pin   = Config.ledPin;
    uint16_t count = Config.ledCount;

    // Reinitialise Adafruit_NeoPixel with current settings
    _strip.updateLength(count);
    _strip.setPin(pin);
    _strip.begin();
    _strip.setBrightness(Config.ledBrightness);
    _strip.clear();
    _strip.show();

    _initialised = true;
    _initPin     = pin;
    _initCount   = count;

    Serial.printf("[LED] NeoPixel init: pin=%d  count=%d  brightness=%d\n",
                  pin, count, Config.ledBrightness);
}

void LEDController::clearStrip() {
    _strip.clear();
    _strip.show();
}

uint32_t LEDController::scaledColor(float brightness) {
    uint8_t r = (uint8_t)(Config.ledR * brightness);
    uint8_t g = (uint8_t)(Config.ledG * brightness);
    uint8_t b = (uint8_t)(Config.ledB * brightness);
    return _strip.Color(r, g, b);
}

void LEDController::setTrigger(bool active) {
    _active = active;
}

void LEDController::testPulse(uint32_t durationMs) {
    _testMode  = true;
    _testEndMs = millis() + durationMs;
}

void LEDController::loop() {
    if (!_initialised) return;

    uint32_t now = millis();

    // Reinit if pin or count changed (requires reboot to take effect cleanly)
    if (_initPin != Config.ledPin || _initCount != Config.ledCount) {
        begin();
    }

    // Update brightness from config each cycle (low overhead)
    _strip.setBrightness(Config.ledBrightness);

    // Test mode
    if (_testMode) {
        if (now < _testEndMs) {
            _active = true;
        } else {
            _testMode = false;
            _active   = false;
        }
    }

    if (!_active) {
        if (_ledState) {
            clearStrip();
            _ledState = false;
        }
        return;
    }

    updateStrip();
}

void LEDController::updateStrip() {
    uint32_t now  = millis();
    uint32_t half = Config.flashSpeedMs / 2;

    switch (Config.flashMode) {
        case FLASH_SOLID: {
            uint32_t col = scaledColor(1.0f);
            for (int i = 0; i < _strip.numPixels(); i++) _strip.setPixelColor(i, col);
            _strip.show();
            _ledState = true;
            break;
        }

        case FLASH_BLINK:
            if (now - _lastFlipMs >= half) {
                _lastFlipMs = now;
                _ledState   = !_ledState;
            }
            if (_ledState) {
                uint32_t col = scaledColor(1.0f);
                for (int i = 0; i < _strip.numPixels(); i++) _strip.setPixelColor(i, col);
            } else {
                _strip.clear();
            }
            _strip.show();
            break;

        case FLASH_PULSE: {
            // Sine-wave brightness ramp (full period = flashSpeedMs)
            uint32_t period = max((uint32_t)100, (uint32_t)Config.flashSpeedMs);
            float phase = (float)(now % period) / (float)period;  // 0.0-1.0
            float bright = (sinf(phase * 2.0f * M_PI - M_PI / 2.0f) + 1.0f) * 0.5f;
            uint32_t col = scaledColor(bright);
            for (int i = 0; i < _strip.numPixels(); i++) _strip.setPixelColor(i, col);
            _strip.show();
            _ledState = true;
            break;
        }

        case FLASH_STROBE: {
            uint32_t period = max((uint32_t)50, (uint32_t)Config.flashSpeedMs);
            uint32_t onTime = period / 8;
            bool on = ((now % period) < onTime);
            if (on != _ledState) {
                _ledState = on;
                if (on) {
                    uint32_t col = scaledColor(1.0f);
                    for (int i = 0; i < _strip.numPixels(); i++) _strip.setPixelColor(i, col);
                } else {
                    _strip.clear();
                }
                _strip.show();
            }
            break;
        }

        default:
            break;
    }
}
