#include "MoodLightManager.h"
#include <math.h>

MoodLightManager::MoodLightManager()
    : strip(MOOD_LED_COUNT, PIN_MOOD_LED, NEO_GRB + NEO_KHZ800),
      effect(LightEffect::LIGHT_OFF),
      r(0), g(0), b(0),
      pulsePeriodMs(1500),
      effectStartTime(0) {}

void MoodLightManager::begin() {
    strip.begin();
    strip.setBrightness(MOOD_LED_BRIGHTNESS);
    strip.show(); // all off
}

void MoodLightManager::render(uint8_t rr, uint8_t gg, uint8_t bb) {
    for (uint16_t i = 0; i < MOOD_LED_COUNT; i++) {
        strip.setPixelColor(i, strip.Color(rr, gg, bb));
    }
    strip.show();
}

void MoodLightManager::setSolidColor(uint8_t rr, uint8_t gg, uint8_t bb) {
    r = rr; g = gg; b = bb;
    effect = LightEffect::SOLID;
    render(r, g, b);
}

void MoodLightManager::setPulseColor(uint8_t rr, uint8_t gg, uint8_t bb, uint16_t periodMs) {
    r = rr; g = gg; b = bb;
    pulsePeriodMs = periodMs;
    effect = LightEffect::PULSE;
    effectStartTime = millis();
}

void MoodLightManager::off() {
    effect = LightEffect::LIGHT_OFF;
    render(0, 0, 0);
}

void MoodLightManager::update() {
    if (effect != LightEffect::PULSE) return;

    unsigned long elapsed = (millis() - effectStartTime) % pulsePeriodMs;
    float phase = (float)elapsed / (float)pulsePeriodMs; // 0..1
    // Triangle-wave brightness scaling gives a smooth breathing pulse
    // without any blocking delay().
    float scale = 0.15f + 0.85f * fabsf(sinf(phase * 2.0f * PI));

    render((uint8_t)(r * scale), (uint8_t)(g * scale), (uint8_t)(b * scale));
}

void MoodLightManager::showEmotion(EvaEmotion emotion) {
    switch (emotion) {
        case EVA_NEUTRAL:    setSolidColor(20, 60, 90);         break; // calm blue
        case EVA_HAPPY:      setSolidColor(255, 170, 0);        break; // warm amber
        case EVA_VERY_HAPPY: setPulseColor(255, 120, 0, 700);   break; // fast pulse amber
        case EVA_CURIOUS:    setPulseColor(0, 200, 200, 1400);  break; // cyan pulse
        case EVA_SCARED:     setPulseColor(255, 0, 60, 400);    break; // fast red pulse
        case EVA_LOL:        setPulseColor(255, 200, 0, 250);   break; // rapid pulse gold
        case EVA_ANGRY:      setSolidColor(255, 0, 0);          break; // solid red
        case EVA_SLEEPY:     setPulseColor(60, 30, 90, 3000);   break; // slow purple breathing
    }
}
