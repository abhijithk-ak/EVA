#pragma once
/*
 * MoodLightManager.h
 * ---------------------------------------------------------
 * Drives the WS2812 mood light. Provides a small palette of
 * named states plus a couple of non-blocking effects (pulse,
 * fade) driven off millis(). Nothing here reads sensors —
 * it is told what to display by BehaviourEngine / Mode
 * classes, keeping the sensor->behaviour->expression pipeline
 * intact.
 * ---------------------------------------------------------
 */

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "Types.h"
#include "Config.h"

enum class LightEffect {
    SOLID,
    PULSE,
    LIGHT_OFF
};

class MoodLightManager {
public:
    MoodLightManager();

    void begin();

    // Call every loop() to advance non-blocking effects.
    void update();

    void setSolidColor(uint8_t r, uint8_t g, uint8_t b);
    void setPulseColor(uint8_t r, uint8_t g, uint8_t b, uint16_t periodMs = 1500);
    void off();

    // Convenience: map an emotion straight to a mood-light look.
    void showEmotion(EvaEmotion emotion);

private:
    Adafruit_NeoPixel strip;
    LightEffect effect;
    uint8_t r, g, b;
    uint16_t pulsePeriodMs;
    unsigned long effectStartTime;

    void render(uint8_t rr, uint8_t gg, uint8_t bb);
};
