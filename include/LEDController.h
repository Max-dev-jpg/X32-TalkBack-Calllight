#pragma once
// =============================================================================
// LEDController.h  –  WS2812B / NeoPixel strip controller (Adafruit NeoPixel)
// =============================================================================

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

class LEDController {
public:
    static LEDController& instance() {
        static LEDController inst;
        return inst;
    }

    // Initialise strip with pin/count from config.  Safe to call again after
    // config change (reinitialises internally).
    void begin();
    void loop();

    void setTrigger(bool active);
    // Override the strip color for the current frame. Call before loop().
    // Falls back to global Config.ledR/G/B if not called each frame.
    void setActiveColor(uint8_t r, uint8_t g, uint8_t b);
    void testPulse(uint32_t durationMs = 2000);

    bool isActive() const { return _active; }

private:
    LEDController() : _strip(0, 0, NEO_GRB + NEO_KHZ800) {}

    // Update physical strip pixels based on current state + flash mode
    void updateStrip();
    void clearStrip();

    // Helpers
    uint32_t scaledColor(float brightness);  // uses _activeR/G/B

    Adafruit_NeoPixel _strip;
    bool     _initialised    = false;
    bool     _active         = false;
    bool     _testMode       = false;
    uint32_t _testEndMs      = 0;

    // Active color (set each frame by main.cpp; defaults to global config)
    uint8_t  _activeR        = 255;
    uint8_t  _activeG        = 120;
    uint8_t  _activeB        = 0;

    // Flash timing
    bool     _ledState       = false;
    uint32_t _lastFlipMs     = 0;
    float    _pulsePhase     = 0.0f;
    uint32_t _lastUpdateMs   = 0;

    // Track config values so we can reinit on change
    uint8_t  _initPin        = 0;
    uint16_t _initCount      = 0;
};
