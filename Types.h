#pragma once
/*
 * Types.h
 * ---------------------------------------------------------
 * Shared enums / plain data types used across modules.
 * Keeping these in one place avoids circular includes
 * between managers and engines.
 * ---------------------------------------------------------
 */

#include <Arduino.h>

// ---------------------------------------------------------
// Emotions — rendered by EmotionEngine on top of RoboEyes
// ---------------------------------------------------------
enum EvaEmotion {
    EVA_NEUTRAL,
    EVA_HAPPY,
    EVA_VERY_HAPPY,
    EVA_CURIOUS,
    EVA_SCARED,
    EVA_LOL,
    EVA_ANGRY,
    EVA_SLEEPY
};

// ---------------------------------------------------------
// Lifecycle / mood states — explicit personality layers.
// These are the living state model that sits above raw sensor
// events and below the concrete rendered expression.
// ---------------------------------------------------------
enum class EvaLifecycleState {
    BOOT,
    IDLE,
    EXPLORING,
    REACTIVE,
    SLEEPING,
    CLOCK,
    RC
};

enum class EvaMoodState {
    MOOD_CALM,
    MOOD_HAPPY,
    MOOD_CURIOUS,
    MOOD_SCARED,
    MOOD_ANGRY,
    MOOD_SLEEPY,
    MOOD_EXCITED,
    MOOD_NEUTRAL
};

// ---------------------------------------------------------
// Behaviours — decided by BehaviourEngine, drives emotion +
// movement together. Sensors never touch these directly.
// ---------------------------------------------------------
enum EvaBehaviour {
    BEHAVIOUR_IDLE,
    BEHAVIOUR_IDLE_FLOURISH,
    BEHAVIOUR_CURIOUS,
    BEHAVIOUR_WANDER,
    BEHAVIOUR_OBSTACLE_AVOID,
    BEHAVIOUR_EDGE_AVOID,
    BEHAVIOUR_TOUCH_REACT,
    BEHAVIOUR_SOUND_REACT,
    BEHAVIOUR_SLEEPY,
    BEHAVIOUR_ASLEEP
};

// ---------------------------------------------------------
// Top level operating modes
// ---------------------------------------------------------
enum EvaMode {
    MODE_EVA,     // Mode 1 - autonomous companion life
    MODE_PET,     // Mode 2 - petting / touch only
    MODE_CLOCK,   // Mode 3 - clock + alarm
    MODE_SLEEP,   // Mode 4 - sleeping
    MODE_RC       // Mode 5 - remote controlled
};

// ---------------------------------------------------------
// Movement primitives — MovementEngine's public vocabulary.
// (Renamed/typed version of the Stage 0.1 prototype defines.)
// ---------------------------------------------------------
enum class MoveState {
    STOP,
    FORWARD,
    BACKWARD,
    TURN_LEFT,
    TURN_RIGHT,
    PIVOT_LEFT,     // in-place left turn
    PIVOT_RIGHT,    // in-place right turn
    CURVE_LEFT,     // forward + left bias
    CURVE_RIGHT     // forward + right bias
};

// ---------------------------------------------------------
// Spatial sensor interpretation (VL53L0X)
// ---------------------------------------------------------
enum class SpatialEvent {
    CLEAR,
    OBSTACLE,
    EDGE
};

// ---------------------------------------------------------
// Touch interpretation
// ---------------------------------------------------------
enum class TouchEvent {
    NONE,
    TAP,
    PETTING,
    LONG_HOLD
};

// ---------------------------------------------------------
// Sound interpretation (environmental awareness only,
// no speech recognition)
// ---------------------------------------------------------
enum class SoundEvent {
    NONE,
    CLAP,
    LOUD_NOISE,
    MUSIC_LIKE
};
