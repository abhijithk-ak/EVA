#pragma once
/*
 * ModeEva.h
 * ---------------------------------------------------------
 * Mode 1 - the main companion mode. Full BehaviourEngine
 * active: idle/curiosity cycle, spatial awareness, touch and
 * sound reactions, emotional cycles, and the transition into
 * Sleep mode once EVA has been idle long enough (or is held
 * for a long touch).
 *
 * Header-only template so it can hold a BehaviourEngine<EmotionT>
 * of whatever concrete EmotionEngine type the sketch instantiates,
 * without this file needing to know about RoboEyes/display types.
 * ---------------------------------------------------------
 */

#include <Arduino.h>
#include "Mode.h"
#include "Types.h"
#include "SensorManager.h"
#include "TouchManager.h"
#include "SoundManager.h"

template <typename BehaviourT>
class ModeEva : public Mode {
public:
    ModeEva(BehaviourT &behaviourRef,
            SensorManager &sensorRef,
            TouchManager &touchRef,
            SoundManager &soundRef,
            bool &sleepRequestFlag)
        : behaviour(behaviourRef),
          sensor(sensorRef),
          touch(touchRef),
          sound(soundRef),
          sleepRequested(sleepRequestFlag) {}

    void enter() override {
        behaviour.setMovementEnabled(true);
        behaviour.clearSleepRequest();
    }

    void update() override {
        sensor.update();
        touch.update();
        sound.update();

        behaviour.update(sensor.getEvent(), touch.getEvent(), sound.getEvent());

        if (behaviour.wantsSleep()) {
            sleepRequested = true; // ModeEngine reads this and switches mode
        }
    }

    void exit() override {
        // Nothing persistent to tear down; BehaviourEngine resets its
        // own state via clearSleepRequest()/enter() next time.
    }

    EvaMode id() const override { return MODE_EVA; }

private:
    BehaviourT &behaviour;
    SensorManager &sensor;
    TouchManager &touch;
    SoundManager &sound;
    bool &sleepRequested;
};
