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
    EVA_SLEEPY,
    // Expanded vocabulary
    EVA_ANNOYED,
    EVA_AFFECTIONATE,
    EVA_SHY,
    EVA_EXCITED,
    EVA_STARTLED,
    EVA_CONFUSED,
    EVA_BORED,
    EVA_PROUD,
    EVA_SUSPICIOUS,
    EVA_SAD,
    EVA_SKEPTIC,
    EVA_WORRIED,
    EVA_FOCUSED,
    EVA_SURPRISED,
    EVA_FRUSTRATED,
    EVA_UNIMPRESSED,
    EVA_SQUINT,
    EVA_FURIOUS,
    EVA_AWE
};


// ---------------------------------------------------------
// Continuous EMO-Style Internal State
// ---------------------------------------------------------
struct EmotionState {
    float valence = 0.0f;   // -1.0 (unpleasant) .. +1.0 (pleasant)
    float arousal = 0.2f;   //  0.0 (calm/sleepy) .. 1.0 (excited/alert)
    float trust   = 0.5f;   //  0.0 (wary) .. 1.0 (affectionate) — builds slowly
    float boredom = 0.0f;   //  0.0 .. 1.0, rises when idle
};


// ---------------------------------------------------------
// Emotion Category — the organic lifestyle buckets used by
// the drive-based lifecycle planner in BehaviourEngine.
// Each category holds a pool of sub-emotions EVA cycles
// through while in that mood phase.
// Plain enum (not class) so values are unqualified like EvaEmotion.
// ---------------------------------------------------------
enum EmotionCategory {
    CAT_ENERGETIC,     // excited, very_happy, happy, awe, curious
    CAT_CONTENT,       // happy, affectionate, proud, curious, focused
    CAT_PENSIVE,       // curious, focused, skeptic, suspicious, squint
    CAT_MELANCHOLY,    // sad, worried, unimpressed, shy, bored
    CAT_IRRITABLE,     // annoyed, frustrated, confused, angry
    CAT_WINDING_DOWN,  // bored, sleepy, neutral, unimpressed, shy
    CAT_SHORT_BURST    // reactive-only: startled, lol, surprised, furious, scared
};

// ---------------------------------------------------------
// Lifecycle / mood states — explicit personality layers.
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
// Behaviours — decided by BehaviourEngine
// ---------------------------------------------------------
enum EvaBehaviour {
    BEHAVIOUR_IDLE,
    BEHAVIOUR_IDLE_FLOURISH,
    BEHAVIOUR_CURIOUS,
    BEHAVIOUR_WANDER,
    BEHAVIOUR_OBSTACLE_AVOID,
    BEHAVIOUR_EDGE_AVOID,
    BEHAVIOUR_TOUCH_REACT,
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
    CURVE_RIGHT,    // forward + right bias
    WIGGLE,         // joyful body shake
    DONUT,          // fast 360 spin
    HOP_FORWARD,    // short energetic hop forward
    FLINCH_BACK,    // sharp fast backward startle
    JERK_STEPS,     // sudden front & back pulse
    SLOW_FORWARD,   // gentle slow forward crawl
    SLOW_CURVE_FWD, // slow curved forward (one wheel slower)
    SLOW_CURVE_REV  // slow curved backward (one wheel slower)
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
    DOUBLE_TAP,
    PETTING,
    LONG_HOLD
};

// SoundEvent removed in V3 — MAX9812 mic eliminated.
// TouchEvent (below) is kept for capacitive touch reactions.
