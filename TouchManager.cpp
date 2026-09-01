#include "TouchManager.h"
#include "Logger.h"

TouchManager::TouchManager()
    : lastPollTime(0),
      baseline(1200.0f),   // sane starting point per observed idle readings
      touched(false),
      touchStartTime(0),
      longHoldFired(false),
      pettingFired(false),
      lastEvent(TouchEvent::NONE) {}

void TouchManager::begin() {
    // Seed the baseline with a handful of real readings at boot,
    // assuming the plate is not touched during power-on.
    float sum = 0;
    const int samples = 20;
    for (int i = 0; i < samples; i++) {
        sum += touchRead(PIN_TOUCH);
        delay(2); // one-time, boot-only settling delay - not runtime behaviour
    }
    baseline = sum / samples;
    EVA_LOGF("[TouchManager] baseline seeded at %.1f\n", baseline);
}

uint16_t TouchManager::readRaw() {
    return touchRead(PIN_TOUCH);
}

void TouchManager::update() {
    unsigned long now = millis();
    if (now - lastPollTime < TOUCH_POLL_INTERVAL_MS) return;
    lastPollTime = now;

    uint16_t raw = readRaw();
    bool isTouchedNow = raw < (baseline * TOUCH_TRIGGER_RATIO);

    lastEvent = TouchEvent::NONE;

    if (!isTouchedNow) {
        // Only drift the baseline while NOT touched, so a held
        // touch never "trains" the baseline to think it's normal.
        baseline = (baseline * (1.0f - TOUCH_BASELINE_ALPHA)) + (raw * TOUCH_BASELINE_ALPHA);
    }

    if (isTouchedNow && !touched) {
        // Touch just started.
        touched = true;
        touchStartTime = now;
        longHoldFired = false;
        pettingFired = false;
    } else if (!isTouchedNow && touched) {
        // Touch just released.
        unsigned long duration = now - touchStartTime;
        touched = false;
        if (!longHoldFired && duration <= TOUCH_TAP_MAX_MS) {
            lastEvent = TouchEvent::TAP;
        }
        // If it was petting/long-hold, those already fired while held.
    } else if (isTouchedNow && touched) {
        unsigned long duration = now - touchStartTime;

        if (!longHoldFired && duration >= TOUCH_LONG_HOLD_MS) {
            longHoldFired = true;
            lastEvent = TouchEvent::LONG_HOLD;
        } else if (!pettingFired && duration >= TOUCH_PET_MIN_MS && duration <= TOUCH_PET_MAX_MS) {
            pettingFired = true;
            lastEvent = TouchEvent::PETTING;
        }
    }
}

TouchEvent TouchManager::getEvent() {
    TouchEvent ev = lastEvent;
    lastEvent = TouchEvent::NONE; // consume-once: see header comment
    return ev;
}

bool TouchManager::isTouched() const {
    return touched;
}

unsigned long TouchManager::getCurrentTouchDurationMs() const {
    if (!touched) return 0;
    return millis() - touchStartTime;
}
