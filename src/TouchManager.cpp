#include "TouchManager.h"
#include "Logger.h"

TouchManager::TouchManager()
    : lastPollTime(0),
      baseline(1000.0f),
      touched(false),
      justPressed(false),
      justReleased(false),
      touchStartTime(0),
      longHoldFired(false),
      pettingFired(false),
      waitingForSecondTap(false),
      firstTapTime(0),
      lastEvent(TouchEvent::NONE) {}

void TouchManager::begin() {
    float sum = 0;
    const int samples = 25;
    for (int i = 0; i < samples; i++) {
        sum += touchRead(PIN_TOUCH);
        delay(2);
    }
    baseline = sum / samples;
    EVA_LOGF("[TouchManager] Baseline seeded at %.1f\n", baseline);
}

uint16_t TouchManager::readRaw() {
    uint16_t r1 = touchRead(PIN_TOUCH);
    delayMicroseconds(50);
    uint16_t r2 = touchRead(PIN_TOUCH);
    delayMicroseconds(50);
    uint16_t r3 = touchRead(PIN_TOUCH);

    // 3-sample median selection
    if ((r1 >= r2 && r1 <= r3) || (r1 >= r3 && r1 <= r2)) return r1;
    if ((r2 >= r1 && r2 <= r3) || (r2 >= r3 && r2 <= r1)) return r2;
    return r3;
}


void TouchManager::update() {
    unsigned long now = millis();
    if (now - lastPollTime < TOUCH_POLL_INTERVAL_MS) return;
    lastPollTime = now;

    uint16_t raw = readRaw();

    // Dual-sensitivity rule: works cleanly on both USB and floating battery ground
    bool isTouchedNow = (raw < (baseline * TOUCH_TRIGGER_RATIO)) ||
                        ((baseline > raw) && ((baseline - raw) >= TOUCH_TRIGGER_DELTA));

    // Slow baseline drift ONLY when untouched
    if (!isTouchedNow) {
        baseline = (baseline * (1.0f - TOUCH_BASELINE_ALPHA)) + (raw * TOUCH_BASELINE_ALPHA);
    }

    // Check for double-tap window expiration
    if (waitingForSecondTap && (now - firstTapTime > TOUCH_DOUBLE_TAP_WINDOW_MS)) {
        waitingForSecondTap = false;
        lastEvent = TouchEvent::TAP;
    }

    if (isTouchedNow && !touched) {
        // Touch just started
        touched = true;
        justPressed = true;
        touchStartTime = now;
        longHoldFired = false;
        pettingFired = false;
    } else if (!isTouchedNow && touched) {
        // Touch just released
        unsigned long duration = now - touchStartTime;
        touched = false;
        justReleased = true;


        if (!longHoldFired && !pettingFired && duration >= 50 && duration <= TOUCH_TAP_MAX_MS) {
            if (waitingForSecondTap && (now - firstTapTime <= TOUCH_DOUBLE_TAP_WINDOW_MS)) {
                // Second tap arrived within window -> DOUBLE_TAP!
                waitingForSecondTap = false;
                lastEvent = TouchEvent::DOUBLE_TAP;
            } else {
                // First tap -> start window
                waitingForSecondTap = true;
                firstTapTime = now;
            }
        }
    } else if (isTouchedNow && touched) {
        unsigned long duration = now - touchStartTime;

        if (!longHoldFired && duration >= TOUCH_LONG_HOLD_MS) {
            longHoldFired = true;
            waitingForSecondTap = false;
            lastEvent = TouchEvent::LONG_HOLD;
        } else if (!pettingFired && duration >= TOUCH_PET_MIN_MS && duration <= TOUCH_PET_MAX_MS) {
            pettingFired = true;
            waitingForSecondTap = false;
            lastEvent = TouchEvent::PETTING;
        }
    }
}

TouchEvent TouchManager::getEvent() {
    TouchEvent ev = lastEvent;
    lastEvent = TouchEvent::NONE;
    return ev;
}

bool TouchManager::isTouched() const {
    return touched;
}

unsigned long TouchManager::getCurrentTouchDurationMs() const {
    if (!touched) return 0;
    return millis() - touchStartTime;
}
