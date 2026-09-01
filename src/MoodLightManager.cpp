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
    float phase = (float)elapsed / (float)pulsePeriodMs;
    float scale = 0.15f + 0.85f * fabsf(sinf(phase * 2.0f * PI));

    render((uint8_t)(r * scale), (uint8_t)(g * scale), (uint8_t)(b * scale));
}

void MoodLightManager::showEmotion(EvaEmotion emotion) {
    switch (emotion) {
        case EVA_NEUTRAL:     setSolidColor(20, 60, 90);         break; // calm blue
        case EVA_HAPPY:       setSolidColor(255, 170, 0);        break; // warm amber
        case EVA_VERY_HAPPY:  setPulseColor(255, 120, 0, 700);   break; // fast pulse amber
        case EVA_CURIOUS:     setPulseColor(0, 200, 200, 1400);  break; // cyan pulse
        case EVA_SCARED:      setPulseColor(255, 0, 60, 400);    break; // fast red pulse
        case EVA_LOL:         setPulseColor(255, 200, 0, 250);   break; // rapid pulse gold
        case EVA_ANGRY:       setSolidColor(255, 0, 0);          break; // solid red
        case EVA_SLEEPY:      setPulseColor(60, 30, 90, 3000);   break; // slow purple breathing
        case EVA_ANNOYED:     setSolidColor(255, 120, 0);        break; // amber flash
        case EVA_AFFECTIONATE:setPulseColor(255, 105, 180, 2000); break; // soft pink pulse
        case EVA_SHY:         setPulseColor(80, 80, 140, 2500);  break; // dim soft purple
        case EVA_EXCITED:     setPulseColor(255, 215, 0, 500);   break; // fast gold pulse
        case EVA_STARTLED:    setSolidColor(255, 0, 0);          break; // sharp red flash
        case EVA_CONFUSED:    setPulseColor(0, 255, 128, 800);   break; // teal pulse
        case EVA_BORED:       setPulseColor(40, 60, 80, 4000);   break; // dim blue slow pulse
        case EVA_PROUD:       setPulseColor(255, 215, 0, 1200);  break; // gold pulse
        case EVA_SUSPICIOUS:  setSolidColor(180, 180, 200);      break; // pale held steady
        case EVA_SAD:         setPulseColor(0, 50, 180, 3500);   break; // dim blue fade
    }
}
