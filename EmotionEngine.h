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
 * Templated on the eyes type so it works with any RoboEyes<T>
 * instantiation (e.g. RoboEyes<Adafruit_SSD1306>) without this
 * file needing to know about the display class.
 *
 * Persistent base emotion + temporary timed effects (sweat,
 * flicker) are layered independently, using millis() — never
 * delay().
 * ---------------------------------------------------------
 */

#include <Arduino.h>
#include "Types.h"

template <typename EyesT>
class EmotionEngine {
public:
    explicit EmotionEngine(EyesT &eyesRef)
        : eyes(eyesRef),
          currentEmotion(EVA_NEUTRAL),
          emotionChangedAt(millis()),
          emotionDecayMs(8000),
          sweatActive(false), sweatStartTime(0), sweatDuration(0),
          flickerActive(false), flickerStartTime(0), flickerDuration(0) {}

    // Must be called every loop() — advances temporary effect timers.
    void update() {
        updateSweat();
        updateFlicker();
        decayEmotion();
    }

    // ---------------- Persistent base emotions ----------------

    void setNeutral() {
        setEmotionState(EVA_NEUTRAL, DEFAULT, OFF, OFF, 5000);
    }

    void setHappy() {
        setEmotionState(EVA_HAPPY, HAPPY, OFF, OFF, 9000);
    }

    void setVeryHappy() {
        setEmotionState(EVA_VERY_HAPPY, HAPPY, OFF, OFF, 10000);
        eyes.anim_laugh();
    }

    void setCurious() {
        setEmotionState(EVA_CURIOUS, DEFAULT, ON, OFF, 12000);
    }

    void setScared() {
        setEmotionState(EVA_SCARED, TIRED, OFF, OFF, 15000);
        startSweat(5000);
        startFlicker(3000);
    }

    void setLol() {
        setEmotionState(EVA_LOL, HAPPY, OFF, ON, 12000);
        eyes.anim_laugh();
    }

    void setAngry() {
        setEmotionState(EVA_ANGRY, ANGRY, OFF, OFF, 15000);
    }

    void setSleepy() {
        setEmotionState(EVA_SLEEPY, TIRED, OFF, OFF, 20000);
    }

    // One-shot reaction, does not change persistent base emotion.
    void playConfused() {
        eyes.anim_confused();
    }

    void blinkNow() {
        eyes.blink();
    }

    void setFromBehaviour(EvaBehaviour behaviour) {
        switch (behaviour) {
            case BEHAVIOUR_IDLE:
            case BEHAVIOUR_IDLE_FLOURISH:
                setNeutral();
                break;
            case BEHAVIOUR_CURIOUS:
            case BEHAVIOUR_WANDER:
                setCurious();
                break;
            case BEHAVIOUR_TOUCH_REACT:
                setHappy();
                break;
            case BEHAVIOUR_SOUND_REACT:
                setVeryHappy();
                break;
            case BEHAVIOUR_OBSTACLE_AVOID:
            case BEHAVIOUR_EDGE_AVOID:
                setScared();
                break;
            case BEHAVIOUR_SLEEPY:
            case BEHAVIOUR_ASLEEP:
                setSleepy();
                break;
            default:
                setNeutral();
                break;
        }
    }

    EvaEmotion getEmotion() const {
        return currentEmotion;
    }

private:
    EyesT &eyes;
    EvaEmotion currentEmotion;
    unsigned long emotionChangedAt;
    unsigned long emotionDecayMs;

    void setEmotionState(EvaEmotion em, int mood, bool curiosityEnabled, bool cyclopsEnabled, unsigned long decayMs) {
        currentEmotion = em;
        emotionChangedAt = millis();
        emotionDecayMs = decayMs;
        eyes.setMood(mood);
        eyes.setCuriosity(curiosityEnabled ? ON : OFF);
        eyes.setCyclops(cyclopsEnabled ? ON : OFF);
    }

    void decayEmotion() {
        if (currentEmotion == EVA_NEUTRAL) return;
        if ((millis() - emotionChangedAt) >= emotionDecayMs) {
            setNeutral();
        }
    }

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
