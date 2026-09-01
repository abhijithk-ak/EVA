#pragma once
/*
 * BehaviourEngine.h
 * ---------------------------------------------------------
 * EVA's core "is it alive?" state machine. This is the ONLY
 * place in the firmware allowed to turn a sensor interpretation
 * into an emotion or a movement. Nothing else may call
 * MovementEngine::driveFor() or EmotionEngine::setXxx() as a
 * direct reaction to a raw sensor value.
 *
 *   Sensor -> Interpretation (Sensor/Touch/SoundManager)
 *          -> Behaviour       (THIS CLASS)
 *          -> Emotion + Movement + Light + Sound
 *
 * Templated on the EmotionEngine type so it stays decoupled
 * from the concrete RoboEyes<Display> instantiation, matching
 * the pattern already established in the project reference:
 *
 *   EmotionEngine<decltype(roboEyes)> emotion(roboEyes);
 *   BehaviourEngine<decltype(emotion)> behaviour(emotion, ...);
 *
 * Reused by Mode 1 (EVA) with movement enabled, and Mode 2
 * (Pet) with movement disabled — Modes decide which
 * capabilities are active, per the target architecture.
 * ---------------------------------------------------------
 */

#include <Arduino.h>
#include "Types.h"
#include "Config.h"
#include "MovementEngine.h"
#include "MoodLightManager.h"
#include "BuzzerManager.h"

template <typename EmotionT>
class BehaviourEngine {
public:
    BehaviourEngine(EmotionT &emotionRef,
                     MovementEngine &movementRef,
                     MoodLightManager &lightRef,
                     BuzzerManager &buzzerRef)
        : emotion(emotionRef),
          movement(movementRef),
          light(lightRef),
          buzzer(buzzerRef),
          state(BEHAVIOUR_IDLE),
          lifecycle(EvaLifecycleState::BOOT),
          currentMood(EvaMoodState::MOOD_CALM),
          stateEnteredAt(0),
          nextStateDelay(randomIdleDelay()),
          movementEnabled(true),
          movementSuppressedUntilTouch(false),
          continuousIdleStart(0),
          sleepyRequested(false),
          sleepyThresholdMs(randomSleepyThreshold()) {
        // Do not touch hardware here: RoboEyes / NeoPixel / buzzer need
        // their begin() routines to run first in setup().
    }

    void setMovementEnabled(bool enabled) {
        movementEnabled = enabled;
    }

    // Called after waking from Sleep Mode: EVA comes back up calm and
    // stationary (as if in Pet Mode) and only starts moving again once
    // the person actually touches it - it should not immediately dash
    // off right after waking on its own.
    void wakeCalm() {
        movementSuppressedUntilTouch = true;
    }

    // Feed the latest interpreted sensor events every loop().
    // Pass SpatialEvent::CLEAR / TouchEvent::NONE / SoundEvent::NONE
    // for any sensor a mode chooses not to use.
    void update(SpatialEvent spatial, TouchEvent touch, SoundEvent sound) {
        unsigned long now = millis();

        // Reactive events take priority over the idle/curious cycle,
        // but they are still interpreted here — never applied directly
        // by the sensor manager itself.
        if (handleReactiveEvents(spatial, touch, sound, now)) {
            emotion.update();
            light.update();
            buzzer.update();
            movement.update();
            return;
        }

        // A previous reaction's busy window just elapsed - settle
        // back to idle before resuming the normal life cycle.
        if (isReactiveState(state)) {
            returnToIdle(now);
        }

        switch (state) {
            case BEHAVIOUR_IDLE:
                updateIdle(now);
                break;
            case BEHAVIOUR_CURIOUS:
                updateCurious(now);
                break;
            case BEHAVIOUR_WANDER:
                updateWander(now);
                break;
            case BEHAVIOUR_SLEEPY:
                // Held here until the owning Mode acts on wantsSleep().
                break;
            default:
                // Reactive states resolve themselves via their timers
                // inside handleReactiveEvents(); nothing to do here.
                break;
        }

        emotion.update();
        light.update();
        buzzer.update();
        movement.update();
    }

    EvaBehaviour getState() const { return state; }
    EvaLifecycleState getLifecycle() const { return lifecycle; }
    EvaMoodState getMood() const { return currentMood; }

    // ModeEva polls this to know when to hand off control to Sleep mode.
    bool wantsSleep() const { return sleepyRequested; }
    void clearSleepRequest() {
        sleepyRequested = false;
        continuousIdleStart = millis();
        sleepyThresholdMs = randomSleepyThreshold();
    }

    void setLifecycle(EvaLifecycleState nextLifecycle) {
        lifecycle = nextLifecycle;
    }

private:
    EmotionT &emotion;
    MovementEngine &movement;
    MoodLightManager &light;
    BuzzerManager &buzzer;

    EvaBehaviour state;
    EvaLifecycleState lifecycle;
    EvaMoodState currentMood;
    unsigned long stateEnteredAt;
    unsigned long nextStateDelay;
    bool movementEnabled;
    bool movementSuppressedUntilTouch;

    unsigned long continuousIdleStart;
    bool sleepyRequested;
    unsigned long sleepyThresholdMs;

    // Reactive-state bookkeeping (obstacle/edge/touch/sound all share
    // this single "busy until" timer so only one reaction runs at once,
    // keeping behaviour readable and intentional rather than chaotic).
    unsigned long reactiveBusyUntil = 0;

    static bool isReactiveState(EvaBehaviour s) {
        return s == BEHAVIOUR_OBSTACLE_AVOID || s == BEHAVIOUR_EDGE_AVOID ||
               s == BEHAVIOUR_TOUCH_REACT || s == BEHAVIOUR_SOUND_REACT ||
               s == BEHAVIOUR_IDLE_FLOURISH;
    }

    static unsigned long randomIdleDelay() {
        return random(BEHAVIOUR_IDLE_MIN_MS, BEHAVIOUR_IDLE_MAX_MS);
    }

    static unsigned long randomCuriousDelay() {
        return random(BEHAVIOUR_CURIOUS_MIN_MS, BEHAVIOUR_CURIOUS_MAX_MS);
    }

    static unsigned long randomSleepyThreshold() {
        return random(BEHAVIOUR_SLEEPY_AFTER_MIN_MS, BEHAVIOUR_SLEEPY_AFTER_MAX_MS);
    }

    // 5 + random(5) seconds, i.e. 5000-10000ms - how long a touch/sound
    // reaction's emotion stays visible before EVA settles back to idle.
    static unsigned long randomReactionDwell() {
        return random(BEHAVIOUR_REACTION_DWELL_MIN_MS, BEHAVIOUR_REACTION_DWELL_MAX_MS);
    }

    bool canMove() const {
        return movementEnabled && !movementSuppressedUntilTouch;
    }

    void enterState(EvaBehaviour newState, unsigned long now) {
        state = newState;
        stateEnteredAt = now;
        lifecycle = (newState == BEHAVIOUR_SLEEPY || newState == BEHAVIOUR_ASLEEP)
            ? EvaLifecycleState::SLEEPING
            : (newState == BEHAVIOUR_CURIOUS || newState == BEHAVIOUR_WANDER)
                ? EvaLifecycleState::EXPLORING
                : (newState == BEHAVIOUR_OBSTACLE_AVOID || newState == BEHAVIOUR_EDGE_AVOID ||
                   newState == BEHAVIOUR_TOUCH_REACT || newState == BEHAVIOUR_SOUND_REACT)
                    ? EvaLifecycleState::REACTIVE
                    : EvaLifecycleState::IDLE;
    }

    static EvaMoodState moodForEmotion(EvaEmotion emotion) {
        switch (emotion) {
            case EVA_HAPPY: return EvaMoodState::MOOD_HAPPY;
            case EVA_VERY_HAPPY: return EvaMoodState::MOOD_EXCITED;
            case EVA_CURIOUS: return EvaMoodState::MOOD_CURIOUS;
            case EVA_SCARED: return EvaMoodState::MOOD_SCARED;
            case EVA_ANGRY: return EvaMoodState::MOOD_ANGRY;
            case EVA_SLEEPY: return EvaMoodState::MOOD_SLEEPY;
            case EVA_NEUTRAL: return EvaMoodState::MOOD_CALM;
            case EVA_LOL: return EvaMoodState::MOOD_EXCITED;
        }
        return EvaMoodState::MOOD_CALM;
    }

    void syncMood(EvaEmotion emotionState) {
        EvaMoodState nextMood = moodForEmotion(emotionState);
        if (currentMood != nextMood) {
            currentMood = nextMood;
        }
    }

    bool inReactiveCooldown(unsigned long now) const {
        return now < reactiveBusyUntil;
    }

    // Returns true if a reactive event was handled this tick (caller
    // should skip the idle/curious cycle for this tick).
    bool handleReactiveEvents(SpatialEvent spatial, TouchEvent touch,
                               SoundEvent sound, unsigned long now) {
        if (inReactiveCooldown(now)) {
            return true; // still busy with a previous reaction
        }

        // Any real touch clears the post-wake "calm until touched"
        // suppression, whatever else it goes on to trigger below.
        if (touch != TouchEvent::NONE) {
            movementSuppressedUntilTouch = false;
        }

        // Priority: safety (obstacle/edge) > touch > sound.
        if (canMove() && spatial == SpatialEvent::OBSTACLE) {
            reactToObstacle(now);
            return true;
        }
        if (canMove() && spatial == SpatialEvent::EDGE) {
            reactToEdge(now);
            return true;
        }
        if (touch == TouchEvent::TAP) {
            reactToTap(now);
            return true;
        }
        if (touch == TouchEvent::PETTING) {
            reactToPetting(now);
            return true;
        }
        if (touch == TouchEvent::LONG_HOLD) {
            reactToLongHold(now);
            return true;
        }
        if (sound == SoundEvent::CLAP) {
            reactToClap(now);
            return true;
        }
        if (sound == SoundEvent::LOUD_NOISE) {
            reactToLoudNoise(now);
            return true;
        }
        if (sound == SoundEvent::MUSIC_LIKE) {
            reactToMusic(now);
            return true;
        }
        return false;
    }

    // ---------------- Idle / Curious / Wander cycle ----------------

    void updateIdle(unsigned long now) {
        if (continuousIdleStart == 0) continuousIdleStart = now;

        if (now - continuousIdleStart >= sleepyThresholdMs) {
            enterState(BEHAVIOUR_SLEEPY, now);
            emotion.setSleepy();
            syncMood(EVA_SLEEPY);
            light.showEmotion(EVA_SLEEPY);
            sleepyRequested = true;
            return;
        }

        if (now - stateEnteredAt >= nextStateDelay) {
            // "Life cycle" variety: most of the time EVA gets curious
            // and looks around, but sometimes it just plays a brief
            // emotion flourish in place first (a quick Happy/Curious
            // blip) so idle time doesn't feel like a flat two-state
            // metronome between Neutral and Curious.
            if (random(0, 100) < BEHAVIOUR_IDLE_FLOURISH_CHANCE_PCT) {
                playIdleFlourish(now);
                return;
            }

            enterState(BEHAVIOUR_CURIOUS, now);
            nextStateDelay = randomCuriousDelay();
            emotion.setCurious();
            syncMood(EVA_CURIOUS);
            light.showEmotion(EVA_CURIOUS);
        }
    }

    void playIdleFlourish(unsigned long now) {
        // A short, in-place emotion beat - EVA doesn't move, it just
        // briefly "reacts to its own thoughts" before settling again.
        if (random(0, 2) == 0) {
            emotion.setHappy();
            light.showEmotion(EVA_HAPPY);
        } else {
            emotion.setCurious();
            light.showEmotion(EVA_CURIOUS);
        }
        unsigned long dur = random(BEHAVIOUR_IDLE_FLOURISH_MIN_MS, BEHAVIOUR_IDLE_FLOURISH_MAX_MS);
        finishReaction(BEHAVIOUR_IDLE_FLOURISH, now, dur);
    }

    void updateCurious(unsigned long now) {
        if (now - stateEnteredAt >= nextStateDelay) {
            // Look/move, then settle back to idle.
            if (canMove() && random(0, 100) < 60) {
                enterState(BEHAVIOUR_WANDER, now);
                MoveState dir = (random(0, 2) == 0) ? MoveState::CURVE_LEFT : MoveState::CURVE_RIGHT;
                movement.driveFor(dir, MOVE_TURN_MS, MOVE_TURN_SPEED);
                nextStateDelay = MOVE_TURN_MS + MOVE_STOP_SETTLE_MS;
            } else {
                returnToIdle(now);
            }
        }
    }

    void updateWander(unsigned long now) {
        if (!movement.isMoving() && (now - stateEnteredAt >= nextStateDelay)) {
            returnToIdle(now);
        }
    }

    void returnToIdle(unsigned long now) {
        enterState(BEHAVIOUR_IDLE, now);
        nextStateDelay = randomIdleDelay();
        continuousIdleStart = now;
        sleepyThresholdMs = randomSleepyThreshold(); // re-roll each fresh idle stretch
        emotion.setNeutral();
        syncMood(EVA_NEUTRAL);
        light.showEmotion(EVA_NEUTRAL);
    }

    // ---------------- Reactions ----------------

    void reactToObstacle(unsigned long now) {
        emotion.setScared();
        syncMood(EVA_SCARED);
        light.showEmotion(EVA_SCARED);
        buzzer.playForEmotion(EVA_SCARED);
        movement.stop();
        MoveState turn = (random(0, 2) == 0) ? MoveState::PIVOT_LEFT : MoveState::PIVOT_RIGHT;
        movement.driveFor(turn, MOVE_AVOID_TURN_MS, MOVE_TURN_SPEED);
        finishReaction(BEHAVIOUR_OBSTACLE_AVOID, now, MOVE_AVOID_TURN_MS + MOVE_STOP_SETTLE_MS);
    }

    void reactToEdge(unsigned long now) {
        emotion.setScared();
        syncMood(EVA_SCARED);
        light.showEmotion(EVA_SCARED);
        buzzer.playForEmotion(EVA_SCARED);
        movement.stop();
        movement.driveFor(MoveState::BACKWARD, MOVE_BACK_MS, MOVE_DEFAULT_SPEED);
        finishReaction(BEHAVIOUR_EDGE_AVOID, now, MOVE_BACK_MS + MOVE_STOP_SETTLE_MS);
    }

    void reactToTap(unsigned long now) {
        emotion.setHappy();
        syncMood(EVA_HAPPY);
        light.showEmotion(EVA_HAPPY);
        buzzer.playForEmotion(EVA_HAPPY);
        finishReaction(BEHAVIOUR_TOUCH_REACT, now, randomReactionDwell());
    }

    void reactToPetting(unsigned long now) {
        emotion.setAngry();
        syncMood(EVA_ANGRY);
        light.showEmotion(EVA_ANGRY);
        buzzer.playForEmotion(EVA_ANGRY);
        finishReaction(BEHAVIOUR_TOUCH_REACT, now, randomReactionDwell());
    }

    void reactToLongHold(unsigned long now) {
        emotion.setSleepy();
        syncMood(EVA_SLEEPY);
        light.showEmotion(EVA_SLEEPY);
        sleepyRequested = true; // owning Mode transitions to Sleep mode
        finishReaction(BEHAVIOUR_SLEEPY, now, 500);
    }

    void reactToClap(unsigned long now) {
        emotion.setCurious();
        syncMood(EVA_CURIOUS);
        light.showEmotion(EVA_CURIOUS);
        buzzer.playForEmotion(EVA_CURIOUS);
        finishReaction(BEHAVIOUR_SOUND_REACT, now, randomReactionDwell());
    }

    void reactToLoudNoise(unsigned long now) {
        emotion.setScared();
        syncMood(EVA_SCARED);
        light.showEmotion(EVA_SCARED);
        buzzer.playForEmotion(EVA_SCARED);
        finishReaction(BEHAVIOUR_SOUND_REACT, now, randomReactionDwell());
    }

    void reactToMusic(unsigned long now) {
        emotion.setVeryHappy();
        syncMood(EVA_VERY_HAPPY);
        light.showEmotion(EVA_VERY_HAPPY);
        buzzer.playForEmotion(EVA_VERY_HAPPY);
        finishReaction(BEHAVIOUR_SOUND_REACT, now, randomReactionDwell());
    }

    void finishReaction(EvaBehaviour reaction, unsigned long now, unsigned long busyMs) {
        state = reaction;
        stateEnteredAt = now;
        reactiveBusyUntil = now + busyMs;
        // After the busy window elapses, the NEXT update() call will
        // fall through handleReactiveEvents() -> false, and idle/curious
        // resumes on the following tick via returnToIdle().
        nextStateDelay = 0;
    }

    void applyLightForState() {
        light.showEmotion(EVA_NEUTRAL);
    }
};
