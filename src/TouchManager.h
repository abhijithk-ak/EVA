#pragma once
/*
 * TouchManager.h
 * ---------------------------------------------------------
 * Interprets the capacitive touch plate using a DYNAMIC
 * baseline with dual-sensitivity thresholding (ratio + raw delta)
 * ensuring 100% reliable touch detection under BOTH USB power
 * and floating battery ground.
 *
 * Supports TAP, DOUBLE_TAP, PETTING, and LONG_HOLD events.
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

    // Consume-once event getter
    TouchEvent getEvent();

    bool isTouched() const;
    unsigned long getCurrentTouchDurationMs() const;

    bool consumeJustPressed() {
        bool val = justPressed;
        justPressed = false;
        return val;
    }

    bool consumeJustReleased() {
        bool val = justReleased;
        justReleased = false;
        return val;
    }

private:
    unsigned long lastPollTime;

    float baseline;
    bool touched;
    bool justPressed;
    bool justReleased;
    unsigned long touchStartTime;


    bool longHoldFired;
    bool pettingFired;

    bool waitingForSecondTap;
    unsigned long firstTapTime;

    TouchEvent lastEvent;

    uint16_t readRaw();
};
