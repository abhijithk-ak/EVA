#pragma once
/*
 * EmotionEngine.h
 * ---------------------------------------------------------
 * EVA's expression layer, sitting on top of the official
 * FluxGarage RoboEyes library. We never modify RoboEyes —
 * we only combine its official, public API (moods, curiosity,
 * sweat, flicker, cyclops, one-shot animations) into EVA's
 * own richer emotion vocabulary.
 *
 * Provides blink-wrapped smooth transitions (transitionTo)
 * with a non-blocking state machine and single-slot queue,
 * while leaving direct setX() methods available for zero-latency
 * startle/safety reactions.
 * ---------------------------------------------------------
 */

#include <Arduino.h>
#include "Types.h"

enum TransitionPhase {
    TRANSITION_NONE,
    TRANSITION_CLOSING,
    TRANSITION_REOPENING
};

template <typename EyesT>
class EmotionEngine {
public:
    explicit EmotionEngine(EyesT &eyesRef)
        : eyes(eyesRef),
          currentEmotion(EVA_NEUTRAL),
          emotionChangedAt(millis()),
          emotionDecayMs(5000),
          lastUpdate(millis()),
          restingArousal(0.2f),
          decayRateValence(0.12f),
          decayRateArousal(0.18f),
          boredomGrowthRate(0.015f),
          emotionHoldUntil(0),
          transitionPhase(TRANSITION_NONE),
          targetEmotion(EVA_NEUTRAL),
          transitionStartTime(0),
          hasQueuedTarget(false),
          queuedTargetEmotion(EVA_NEUTRAL),
          pendingHoldOverride(0),
          queuedHoldOverride(0),
          sweatActive(false), sweatStartTime(0), sweatDuration(0),
          flickerActive(false), flickerStartTime(0), flickerDuration(0) {}


    // Must be called every loop() — advances internal continuous state,
    // transitions, and temporary effects.
    void update() {
        unsigned long now = millis();
        float dt = (now - lastUpdate) / 1000.0f;
        if (dt > 1.0f || dt < 0.0f) dt = 0.05f; // safety bounds
        lastUpdate = now;

        // Decay toward baseline — this IS the smooth, non-robotic feeling
        state.valence += (0.0f - state.valence) * decayRateValence * dt;
        state.arousal += (restingArousal - state.arousal) * decayRateArousal * dt;
        state.boredom += boredomGrowthRate * dt;
        
        state.boredom = constrain(state.boredom, 0.0f, 1.0f);
        state.valence = constrain(state.valence, -1.0f, 1.0f);
        state.arousal = constrain(state.arousal, 0.0f, 1.0f);
        state.trust   = constrain(state.trust, 0.0f, 1.0f);

        applyStateToExpression();

        updateTransition();
        updateSweat();
        updateFlicker();
        // NOTE: decayEmotion() removed — BehaviourEngine's lifecycle planner
        // now owns all emotion transitions. Emotions hold until the planner
        // deliberately picks the next one, rather than auto-snapping to neutral.
    }

    bool isEmotionHeld() const { return millis() < emotionHoldUntil; }

    // Maps continuous state variables onto RoboEyes public API.
    // Only runs when no explicit emotion hold is active.
    void applyStateToExpression() {
        if (transitionPhase != TRANSITION_NONE) return;
        if (millis() < emotionHoldUntil) return;

        // NEUTRAL gets a special "relaxed/recovering" look — soft and still,
        // like a human just sitting quietly, not the alert DEFAULT gaze.
        if (currentEmotion == EVA_NEUTRAL) {
            eyes.setMood(TIRED);          // droopy but awake — relaxed, not sleepy
            eyes.setCuriosity(OFF);
            eyes.setAutoblinker(ON, 5, 2); // slow, peaceful blinks
            return;
        }

        if (state.arousal < 0.15f)               eyes.setMood(TIRED);
        else if (state.valence > 0.5f)           eyes.setMood(HAPPY);
        else if (state.valence < -0.4f)          eyes.setMood(ANGRY);
        else                                     eyes.setMood(DEFAULT);

        eyes.setCuriosity((state.arousal > 0.4f && state.valence >= 0.0f) ? ON : OFF);

        // Blink rate follows arousal — sleepy = slow blinks, alert = fast
        int blinkBase = map(constrain((int)(state.arousal * 100.0f), 0, 100), 0, 100, 5, 2);
        eyes.setAutoblinker(ON, blinkBase, 2);
    }

    // ---------------- Continuous EMO Event Nudges ----------------

    void onPetShort() {
        state.valence = constrain(state.valence + 0.25f, -1.0f, 1.0f);
        state.arousal = constrain(state.arousal + 0.1f, 0.0f, 1.0f);
        state.boredom = 0.0f;
        transitionTo(EVA_HAPPY);
    }

    void onPetSustained(float seconds) {
        state.valence = constrain(state.valence + 0.02f * seconds, -1.0f, 1.0f);
        state.trust   = constrain(state.trust + 0.01f * seconds, 0.0f, 1.0f);
        state.boredom = 0.0f;
        transitionTo(EVA_AFFECTIONATE);
    }

    void onPetTooRough() {
        state.valence = constrain(state.valence - 0.4f, -1.0f, 1.0f);
        state.arousal = constrain(state.arousal + 0.3f, 0.0f, 1.0f);
        transitionTo(EVA_ANNOYED);
    }

    void onSuddenSound() {
        state.arousal = constrain(state.arousal + 0.5f, 0.0f, 1.0f);
        state.valence = constrain(state.valence - 0.1f, -1.0f, 1.0f);
        transitionTo(EVA_STARTLED);
    }

    void onLongTouchHold() {
        state.arousal = constrain(state.arousal - 0.6f, 0.0f, 1.0f);
        transitionTo(EVA_SLEEPY);
    }

    const EmotionState& getState() const { return state; }
    float getBoredom() const { return state.boredom; }
    void resetBoredom() { state.boredom = 0.0f; }
    float getArousal() const { return state.arousal; }
    float getValence() const { return state.valence; }
    float getTrust() const { return state.trust; }

    // Blink-wrapped smooth transition with optional hold override.
    // holdOverrideMs > 0: after transition completes, emotionHoldUntil is
    // set to now + holdOverrideMs instead of the value baked into set*().
    // This lets BehaviourEngine control how long an emotion is displayed.
    void transitionTo(EvaEmotion target, unsigned long holdOverrideMs = 0) {
        if (target == currentEmotion && transitionPhase == TRANSITION_NONE && holdOverrideMs == 0) return;

        if (transitionPhase != TRANSITION_NONE) {
            queuedTargetEmotion  = target;
            queuedHoldOverride   = holdOverrideMs;
            hasQueuedTarget      = true;
            return;
        }

        pendingHoldOverride  = holdOverrideMs;
        targetEmotion        = target;
        transitionPhase      = TRANSITION_CLOSING;
        transitionStartTime  = millis();
        eyes.close();
    }

    // ---------------- Persistent base emotions (Playful Presets) ----------------

    void setNeutral() {
        state.valence = 0.0f; state.arousal = 0.2f;
        resetEyeGeometry();
        // Neutral = relaxed, recovering — NOT alert. Like a human sitting peacefully.
        // TIRED mood gives a gentle droopy look without being fully sleepy.
        setEmotionState(EVA_NEUTRAL, TIRED, OFF, OFF, 0);
        eyes.setWidth(36, 36); eyes.setHeight(28, 28); eyes.setBorderradius(8, 8);
        eyes.setPosition(S);          // slight downward gaze — restful
        eyes.setAutoblinker(ON, 5, 2);// slow peaceful blinks
        emotionHoldUntil = 0;         // no hold — let drives flow naturally
    }

    void setHappy() {
        state.valence = 0.6f; state.arousal = 0.5f; state.boredom = 0.0f;
        resetEyeGeometry();
        setEmotionState(EVA_HAPPY, HAPPY, OFF, OFF, 9000);
        eyes.setWidth(40, 40); eyes.setHeight(12, 12); eyes.setBorderradius(10, 0);
        emotionHoldUntil = millis() + random(4000, 8000);
    }

    void setVeryHappy() {
        state.valence = 0.9f; state.arousal = 0.8f; state.boredom = 0.0f;
        resetEyeGeometry();
        setEmotionState(EVA_VERY_HAPPY, HAPPY, OFF, OFF, 10000);
        eyes.setWidth(42, 42); eyes.setHeight(10, 10); eyes.setBorderradius(8, 0);
        eyes.anim_laugh();
        emotionHoldUntil = millis() + random(5000, 9000);
    }

    void setCurious() {
        state.valence = 0.3f; state.arousal = 0.6f; state.boredom = 0.0f;
        resetEyeGeometry();
        setEmotionState(EVA_CURIOUS, DEFAULT, ON, OFF, 12000);
        eyes.setWidth(38, 38); eyes.setHeight(38, 38); eyes.setBorderradius(8, 8);
        emotionHoldUntil = millis() + random(4000, 7000);
    }

    void setScared() {
        state.valence = -0.6f; state.arousal = 0.9f;
        resetEyeGeometry();
        setEmotionState(EVA_SCARED, TIRED, OFF, OFF, 15000);
        eyes.setWidth(40, 40); eyes.setHeight(40, 40); eyes.setBorderradius(12, 8);
        startSweat(2500);
        startFlicker(1500); // 1. Flicker active for 1.5 seconds then stops
        emotionHoldUntil = millis() + random(5000, 8000); // 2. Eye shape holds for 5-8s before smooth blink decay
    }


    void setLol() {
        state.valence = 0.8f; state.arousal = 0.7f; state.boredom = 0.0f;
        resetEyeGeometry();
        setEmotionState(EVA_LOL, HAPPY, OFF, ON, 12000);
        eyes.setWidth(40, 40); eyes.setHeight(24, 24); eyes.setBorderradius(8, 8);
        eyes.anim_laugh();
        startFlicker(1500);
        emotionHoldUntil = millis() + random(4000, 8000);
    }

    void setAngry() {
        state.valence = -0.7f; state.arousal = 0.7f;
        resetEyeGeometry();
        setEmotionState(EVA_ANGRY, ANGRY, OFF, OFF, 15000);
        eyes.setWidth(40, 40); eyes.setHeight(20, 20); eyes.setBorderradius(2, 12);
        emotionHoldUntil = millis() + random(4000, 8000);
    }

    void setSleepy() {
        state.valence = 0.0f; state.arousal = 0.05f;
        resetEyeGeometry();
        setEmotionState(EVA_SLEEPY, TIRED, OFF, OFF, 20000);
        eyes.setWidth(38, 38); eyes.setHeight(12, 12); eyes.setBorderradius(4, 4); eyes.setPosition(S);
        emotionHoldUntil = millis() + random(6000, 10000);
    }

    void setAnnoyed() {
        state.valence = -0.4f; state.arousal = 0.5f;
        resetEyeGeometry();
        setEmotionState(EVA_ANNOYED, ANGRY, OFF, OFF, 6000);
        eyes.setWidth(40, 40); eyes.setHeight(12, 12); eyes.setBorderradius(0, 10);
        emotionHoldUntil = millis() + random(4000, 7000);
    }

    void setAffectionate() {
        state.valence = 0.75f; state.arousal = 0.35f; state.trust = min(1.0f, state.trust + 0.05f);
        resetEyeGeometry();
        setEmotionState(EVA_AFFECTIONATE, HAPPY, OFF, OFF, 8000);
        eyes.setWidth(38, 38); eyes.setHeight(14, 14); eyes.setBorderradius(12, 4);
        emotionHoldUntil = millis() + random(4000, 8000);
    }

    void setShy() {
        state.valence = 0.2f; state.arousal = 0.3f;
        resetEyeGeometry();
        setEmotionState(EVA_SHY, TIRED, OFF, OFF, 6000);
        eyes.setWidth(34, 34); eyes.setHeight(28, 28); eyes.setBorderradius(8, 8); eyes.setPosition(S);
        emotionHoldUntil = millis() + random(4000, 7000);
    }

    void setExcited() {
        state.valence = 0.85f; state.arousal = 0.9f; state.boredom = 0.0f;
        resetEyeGeometry();
        setEmotionState(EVA_EXCITED, HAPPY, ON, OFF, 7000);
        eyes.setWidth(44, 44); eyes.setHeight(42, 42); eyes.setBorderradius(12, 12);
        emotionHoldUntil = millis() + random(4000, 8000);
    }

    void setStartled() {
        state.valence = -0.3f; state.arousal = 0.95f;
        resetEyeGeometry();
        setEmotionState(EVA_STARTLED, TIRED, OFF, OFF, 2000);
        eyes.setWidth(44, 44); eyes.setHeight(44, 44); eyes.setBorderradius(16, 16);
        startFlicker(1000);
        emotionHoldUntil = millis() + random(2000, 4000);
    }



    void setConfused() {
        resetEyeGeometry();
        setEmotionState(EVA_CONFUSED, DEFAULT, OFF, OFF, 5000);
        eyes.setBorderradius(8, 2);
        eyes.anim_confused();
    }

    void setBored() {
        resetEyeGeometry();
        setEmotionState(EVA_BORED, TIRED, OFF, OFF, 0); // cleared externally
        eyes.setHeight(eyes.eyeLheightDefault - 4, eyes.eyeRheightDefault - 4);
    }

    void setProud() {
        resetEyeGeometry();
        setEmotionState(EVA_PROUD, HAPPY, OFF, OFF, 6000);
        eyes.setPosition(N);
    }

    void setSuspicious() {
        resetEyeGeometry();
        setEmotionState(EVA_SUSPICIOUS, DEFAULT, OFF, OFF, 5000);
        eyes.setHeight(eyes.eyeLheightDefault, eyes.eyeRheightDefault - 8);
    }

    void setSad() {
        resetEyeGeometry();
        setEmotionState(EVA_SAD, TIRED, OFF, OFF, 10000);
        eyes.setWidth(40, 40); eyes.setHeight(16, 16); eyes.setBorderradius(2, 10); eyes.setPosition(S);
        emotionHoldUntil = millis() + random(5000, 9000);
    }

    void setSkeptic() {
        resetEyeGeometry();
        setEmotionState(EVA_SKEPTIC, DEFAULT, OFF, OFF, 8000);
        eyes.setWidth(40, 40); eyes.setHeight(40, 24); eyes.setBorderradius(8, 2);
        emotionHoldUntil = millis() + random(4000, 8000);
    }

    void setWorried() {
        resetEyeGeometry();
        setEmotionState(EVA_WORRIED, TIRED, OFF, OFF, 8000);
        eyes.setWidth(38, 38); eyes.setHeight(24, 24); eyes.setBorderradius(12, 4); eyes.setPosition(S);
        emotionHoldUntil = millis() + random(4000, 8000);
    }

    void setFocused() {
        resetEyeGeometry();
        setEmotionState(EVA_FOCUSED, DEFAULT, OFF, OFF, 7000);
        eyes.setWidth(32, 32); eyes.setHeight(32, 32); eyes.setBorderradius(2, 2);
        emotionHoldUntil = millis() + random(4000, 7000);
    }

    void setSurprised() {
        resetEyeGeometry();
        setEmotionState(EVA_SURPRISED, DEFAULT, OFF, OFF, 7000);
        eyes.setWidth(44, 44); eyes.setHeight(44, 44); eyes.setBorderradius(16, 16);
        emotionHoldUntil = millis() + random(4000, 7000);
    }

    void setFrustrated() {
        resetEyeGeometry();
        setEmotionState(EVA_FRUSTRATED, ANGRY, OFF, OFF, 8000);
        eyes.setWidth(40, 40); eyes.setHeight(14, 14); eyes.setBorderradius(0, 8);
        emotionHoldUntil = millis() + random(4000, 8000);
    }

    void setUnimpressed() {
        resetEyeGeometry();
        setEmotionState(EVA_UNIMPRESSED, DEFAULT, OFF, OFF, 6000);
        eyes.setWidth(36, 36); eyes.setHeight(16, 16); eyes.setBorderradius(2, 6);
        emotionHoldUntil = millis() + random(4000, 7000);
    }

    void setSquint() {
        resetEyeGeometry();
        setEmotionState(EVA_SQUINT, DEFAULT, OFF, OFF, 6000);
        eyes.setWidth(38, 38); eyes.setHeight(14, 14); eyes.setBorderradius(4, 4);
        emotionHoldUntil = millis() + random(4000, 7000);
    }

    void setFurious() {
        resetEyeGeometry();
        setEmotionState(EVA_FURIOUS, ANGRY, OFF, OFF, 10000);
        eyes.setWidth(44, 44); eyes.setHeight(16, 16); eyes.setBorderradius(0, 14);
        startFlicker(1200);
        emotionHoldUntil = millis() + random(5000, 9000);
    }

    void setAwe() {
        resetEyeGeometry();
        setEmotionState(EVA_AWE, HAPPY, ON, OFF, 9000);
        eyes.setWidth(44, 44); eyes.setHeight(44, 44); eyes.setBorderradius(14, 14);
        emotionHoldUntil = millis() + random(5000, 9000);
    }

    void playConfused(int durationMs = 500) {
        eyes.anim_confused(durationMs);
    }

    void playLaugh(int durationMs = 500) {
        eyes.anim_laugh(durationMs);
    }

    void playDizzy(int durationMs = 800) {
        eyes.anim_dizzy(durationMs);
    }

    void playBounce(int durationMs = 600) {
        eyes.anim_bounce(durationMs);
    }

    void playWink(bool leftEye = true) {
        eyes.anim_wink(leftEye);
    }

    void playSleepyNod() {
        eyes.anim_sleepy_nod();
    }

    void playCuriousLook(unsigned char dir) {
        eyes.anim_curious_look(dir);
    }

    void blinkNow() {
        eyes.blink();
    }

    void setFromBehaviour(EvaBehaviour behaviour) {
        switch (behaviour) {
            case BEHAVIOUR_IDLE:
            case BEHAVIOUR_IDLE_FLOURISH:
                transitionTo(EVA_NEUTRAL);
                break;
            case BEHAVIOUR_CURIOUS:
            case BEHAVIOUR_WANDER:
                transitionTo(EVA_CURIOUS);
                break;
            case BEHAVIOUR_TOUCH_REACT:
                transitionTo(EVA_HAPPY);
                break;
            case BEHAVIOUR_OBSTACLE_AVOID:
            case BEHAVIOUR_EDGE_AVOID:
                setScared(); // direct for safety
                break;
            case BEHAVIOUR_SLEEPY:
            case BEHAVIOUR_ASLEEP:
                transitionTo(EVA_SLEEPY);
                break;
            default:
                transitionTo(EVA_NEUTRAL);
                break;
        }
    }

    EvaEmotion getEmotion() const {
        return currentEmotion;
    }

private:
    EyesT &eyes;
    EmotionState state;
    unsigned long lastUpdate;
    float restingArousal;
    float decayRateValence;
    float decayRateArousal;
    float boredomGrowthRate;
    unsigned long emotionHoldUntil;

    EvaEmotion currentEmotion;

    unsigned long emotionChangedAt;
    unsigned long emotionDecayMs;


    TransitionPhase transitionPhase;
    EvaEmotion targetEmotion;
    unsigned long transitionStartTime;

    bool hasQueuedTarget;
    EvaEmotion queuedTargetEmotion;
    unsigned long pendingHoldOverride;  // applied when transition finishes
    unsigned long queuedHoldOverride;   // carried by the queued target

    void resetEyeGeometry() {
        eyes.setPosition(DEFAULT);
        eyes.setCuriosity(OFF);
        eyes.setCyclops(OFF);
        eyes.setSweat(OFF);
        eyes.setHFlicker(OFF);
        eyes.setVFlicker(OFF);
        eyes.setWidth(36, 36);
        eyes.setHeight(36, 36);
        eyes.setBorderradius(8, 8);
    }

    void setEmotionState(EvaEmotion em, int mood, bool curiosityEnabled, bool cyclopsEnabled, unsigned long decayMs) {
        currentEmotion = em;
        emotionChangedAt = millis();
        emotionDecayMs = decayMs;
        eyes.setMood(mood);
        eyes.setCuriosity(curiosityEnabled ? ON : OFF);
        eyes.setCyclops(cyclopsEnabled ? ON : OFF);
    }

    void applyEmotionDirect(EvaEmotion em) {
        switch (em) {
            case EVA_NEUTRAL:     setNeutral(); break;
            case EVA_HAPPY:       setHappy(); break;
            case EVA_VERY_HAPPY:  setVeryHappy(); break;
            case EVA_CURIOUS:     setCurious(); break;
            case EVA_SCARED:      setScared(); break;
            case EVA_LOL:         setLol(); break;
            case EVA_ANGRY:       setAngry(); break;
            case EVA_SLEEPY:      setSleepy(); break;
            case EVA_ANNOYED:     setAnnoyed(); break;
            case EVA_AFFECTIONATE:setAffectionate(); break;
            case EVA_SHY:         setShy(); break;
            case EVA_EXCITED:     setExcited(); break;
            case EVA_STARTLED:    setStartled(); break;
            case EVA_CONFUSED:    setConfused(); break;
            case EVA_BORED:       setBored(); break;
            case EVA_PROUD:       setProud(); break;
            case EVA_SUSPICIOUS:  setSuspicious(); break;
            case EVA_SAD:         setSad(); break;
            case EVA_SKEPTIC:     setSkeptic(); break;
            case EVA_WORRIED:     setWorried(); break;
            case EVA_FOCUSED:     setFocused(); break;
            case EVA_SURPRISED:   setSurprised(); break;
            case EVA_FRUSTRATED:  setFrustrated(); break;
            case EVA_UNIMPRESSED: setUnimpressed(); break;
            case EVA_SQUINT:      setSquint(); break;
            case EVA_FURIOUS:     setFurious(); break;
            case EVA_AWE:         setAwe(); break;
        }
    }


    void updateTransition() {
        if (transitionPhase == TRANSITION_NONE) {
            if (hasQueuedTarget) {
                hasQueuedTarget = false;
                transitionTo(queuedTargetEmotion, queuedHoldOverride);
                queuedHoldOverride = 0;
            }
            return;
        }

        unsigned long elapsed = millis() - transitionStartTime;

        if (transitionPhase == TRANSITION_CLOSING) {
            bool eyesClosed = (eyes.eyeLheightCurrent <= 4 && eyes.eyeRheightCurrent <= 4);
            if (eyesClosed || elapsed >= 250) {
                applyEmotionDirect(targetEmotion);
                eyes.open();
                transitionPhase = TRANSITION_REOPENING;
                transitionStartTime = millis();
            }
        } else if (transitionPhase == TRANSITION_REOPENING) {
            if (elapsed >= 180) {
                transitionPhase = TRANSITION_NONE;
                // Apply hold override AFTER transition completes, overriding set*()'s default.
                if (pendingHoldOverride > 0) {
                    emotionHoldUntil = millis() + pendingHoldOverride;
                    pendingHoldOverride = 0;
                }
            }
        }
    }

    // decayEmotion() intentionally removed.
    // The old auto-snap-to-neutral after a fixed hold timer made EVA feel
    // robotic. BehaviourEngine's lifecycle planner now owns all emotion
    // transitions, choosing the *next* emotion based on internal drives.


    // ---- temporary sweat effect ----
    bool sweatActive;
    unsigned long sweatStartTime;
    unsigned long sweatDuration;

    void startSweat(unsigned long duration) {
        eyes.setSweat(ON);
        sweatActive = true;
        sweatStartTime = millis();
        sweatDuration = duration;
    }

    void updateSweat() {
        if (sweatActive && (millis() - sweatStartTime >= sweatDuration)) {
            eyes.setSweat(OFF);
            sweatActive = false;
        }
    }

    // ---- temporary flicker effect ----
    bool flickerActive;
    unsigned long flickerStartTime;
    unsigned long flickerDuration;

    void startFlicker(unsigned long duration) {
        eyes.setHFlicker(ON, 2);
        flickerActive = true;
        flickerStartTime = millis();
        flickerDuration = duration;
    }

    void updateFlicker() {
        if (flickerActive && (millis() - flickerStartTime >= flickerDuration)) {
            eyes.setHFlicker(OFF, 0);
            flickerActive = false;
        }
    }
};
