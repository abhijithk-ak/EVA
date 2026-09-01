#pragma once
/*
 * TouchManager.h
 * ---------------------------------------------------------
 * Interprets the capacitive touch plate using a DYNAMIC
 * baseline (exponential moving average of the untouched
 * reading) rather than a single fixed threshold, since raw
 * touchRead() values drift with humidity, temperature and
 * the specific board.
 *
 * Classifies touch duration into TAP / PETTING / LONG_HOLD,
 * matching the Pet Mode lifecycle described in the project
 * reference document. This class only reports events —
 * BehaviourEngine / Mode classes decide what they mean.
 * ---------------------------------------------------------
 */

#include <Arduino.h>
#include "Types.h"
#include "Config.h"

class TouchManager {
public:
    TouchManager();

    void begin();

    // Call every loop(). Rate-limited internally.
    void update();

    // Consume-once: returns the event detected since the last time
    // THIS was called, then clears it. This is deliberately NOT a
    // simple getter - update() is internally rate-limited (polls
    // every TOUCH_POLL_INTERVAL_MS) but loop() runs far faster than
    // that, so a plain getter would hand back the same TAP/PETTING
    // event on every single loop() iteration until the next real
    // poll, and any caller incrementing a value per call (e.g. the
    // Clock alarm-setup UI) would jump by many steps per physical
    // tap. Call this at most once per logical "did something happen"
    // check per loop() iteration.
    TouchEvent getEvent();

    bool isTouched() const;
    unsigned long getCurrentTouchDurationMs() const;

private:
    unsigned long lastPollTime;

    float baseline;
    bool touched;
    unsigned long touchStartTime;

    bool longHoldFired;
    bool pettingFired;

    TouchEvent lastEvent;

    uint16_t readRaw();
};
