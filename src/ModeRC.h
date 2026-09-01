#pragma once
/*
 * ModeRC.h
 * ---------------------------------------------------------
 * Mode 5 - RC. Driving input now arrives through the single
 * shared CommsHub Bluetooth connection (see CommsHub.h/.cpp)
 * instead of a mode-owned BluetoothSerial instance. This mode
 * simply puts EVA into "manual drive" state - autonomous
 * behaviour off, calm idle expression - and guarantees the
 * motors are stopped on entry and exit. CommsHub only ever
 * forwards drive characters to the motors while this mode
 * (MODE_RC) is the active one, and stops the motors itself if
 * the link goes quiet for 1.5s.
 * ---------------------------------------------------------
 */

#include <Arduino.h>
#include "Mode.h"
#include "Types.h"
#include "MovementEngine.h"

template <typename EmotionT>
class ModeRC : public Mode {
public:
    ModeRC(EmotionT &emotionRef, MovementEngine &movementRef)
        : emotion(emotionRef), movement(movementRef) {}

    void enter() override {
        emotion.setNeutral();
        movement.stop();
    }

    void update() override {
        movement.update(); // let any in-flight timed command settle
    }

    void exit() override {
        movement.stop();
    }

    EvaMode id() const override { return MODE_RC; }

private:
    EmotionT &emotion;
    MovementEngine &movement;
};
