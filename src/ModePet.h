#pragma once
/*
 * ModePet.h
 * ---------------------------------------------------------
 * Mode 2 - Pet Mode. Touch interaction and the emotion engine
 * stay active; autonomous movement is disabled and EVA stays
 * physically idle. Reuses the same BehaviourEngine as Mode 1
 * (touch reactions and idle/curious emotional cycling are
 * identical logic) simply with movement turned off, rather
 * than duplicating the state machine.
 *
 * Touch lifecycle (per project spec):
 *   tap / short pet        -> Happy
 *   continuous pet 5-10s   -> Angry cycle
 *   hold > 20s             -> Sleep
 *
 * V3: SoundManager removed — MAX9812 mic eliminated.
 * ---------------------------------------------------------
 */

#include <Arduino.h>
#include "Mode.h"
#include "Types.h"
#include "TouchManager.h"

template <typename BehaviourT>
class ModePet : public Mode {
public:
    ModePet(BehaviourT  &behaviourRef,
            TouchManager &touchRef,
            bool         &sleepRequestFlag,
            bool         &evaRequestFlag)
        : behaviour(behaviourRef),
          touch(touchRef),
          sleepRequested(sleepRequestFlag),
          evaRequested(evaRequestFlag) {}

    void enter() override {
        behaviour.setMovementEnabled(false);
        behaviour.clearSleepRequest();
    }

    void update() override {
        touch.update();

        TouchEvent te = touch.getEvent();
        if (te == TouchEvent::TAP || te == TouchEvent::DOUBLE_TAP || te == TouchEvent::PETTING) {
            evaRequested = true; // Touch or petting in PET mode activates full EVA mode!
        }

        // Spatial sensor intentionally not fed - Pet Mode has no
        // movement, so obstacle/edge interpretation is irrelevant.
        behaviour.update(SpatialEvent::CLEAR, te);

        if (behaviour.wantsSleep()) {
            sleepRequested = true;
        }
    }

    void exit() override {}

    EvaMode id() const override { return MODE_PET; }

private:
    BehaviourT   &behaviour;
    TouchManager &touch;
    bool         &sleepRequested;
    bool         &evaRequested;
};
