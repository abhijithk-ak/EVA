#pragma once
/*
 * =========================================================================
 * EmotionCustomizationGuide.h
 * =========================================================================
 * EVA & FluxGarage RoboEyes Emotion Architecture & Customization Manual
 *
 * This guide explains every method, property, geometry setting, animation,
 * and lifecycle integration point for crafting new custom robot expressions.
 * =========================================================================
 *
 * TABLE OF CONTENTS:
 * 1. FluxGarage RoboEyes API Reference (All Functions & Setters)
 * 2. Anatomy of an Emotion: How to Design & Code a New Emotion
 * 3. Temporary Effects vs Persistent States & Automatic Lifecycle Reversion
 * 4. Categorizing Emotions into EVA's 6 Drive Lifestyle Buckets
 * 5. Step-by-Step Tutorial: Adding a Brand New Emotion End-to-End
 * 6. Ready-to-Use Emotion Recipes
 * =========================================================================
 */

/*
 * =========================================================================
 * 1. FLUXGARAGE ROBOEYES API REFERENCE
 * =========================================================================
 *
 * A. MOOD PRESETS:
 *    - eyes.setMood(DEFAULT);  // Neutral, standard open eyes
 *    - eyes.setMood(TIRED);    // Top eyelids droop downwards (gentle / sad / relaxed)
 *    - eyes.setMood(ANGRY);    // Top eyelids slant inward angrily
 *    - eyes.setMood(HAPPY);    // Bottom curved eyelids raise upwards (happy squint)
 *
 * B. PREDEFINED POSITIONS (Gaze Directions):
 *    - eyes.setPosition(DEFAULT); // Center (X=middle, Y=middle)
 *    - eyes.setPosition(N);       // North (top center)
 *    - eyes.setPosition(NE);      // North-East (top right)
 *    - eyes.setPosition(E);       // East (middle right)
 *    - eyes.setPosition(SE);      // South-East (bottom right)
 *    - eyes.setPosition(S);       // South (bottom center - looking down/sleepy/shy)
 *    - eyes.setPosition(SW);      // South-West (bottom left)
 *    - eyes.setPosition(W);       // West (middle left)
 *    - eyes.setPosition(NW);      // North-West (top left)
 *
 * C. EYE GEOMETRY SETTERS:
 *    - eyes.setWidth(leftWidth, rightWidth);
 *        Default: (36, 36). Example: (44, 44) for wide shocked eyes, or (32, 32) for focused eyes.
 *    - eyes.setHeight(leftHeight, rightHeight);
 *        Default: (36, 36). Example: (12, 12) for narrow squint, (44, 44) for round alert gaze.
 *    - eyes.setBorderradius(leftRadius, rightRadius);
 *        Default: (8, 8). 0 = sharp boxy rectangle, 16 = circular/pill shape.
 *    - eyes.setSpacebetween(pixels);
 *        Default: 10. Space between the left and right eyes (can be adjusted).
 *
 * D. EXPRESSION FLAGS & MACRO ANIMATIONS:
 *    - eyes.setCuriosity(ON / OFF);
 *        When ON, outer eye enlarges dynamically when looking left or right.
 *        (Protected with screen boundary clamping so it never overflows).
 *    - eyes.setCyclops(ON / OFF);
 *        When ON, draws a single centered eye (e.g. for special robot states).
 *    - eyes.setSweat(ON / OFF);
 *        When ON, renders animated dripping sweat drops from the forehead.
 *    - eyes.setHFlicker(ON / OFF, amplitude = 2);
 *        Horizontal vibration / shivering (for fear, anger, confusion).
 *    - eyes.setVFlicker(ON / OFF, amplitude = 5);
 *        Vertical vibration / bobbing (for laughter, excitement, shock).
 *    - eyes.setAutoblinker(ON / OFF, intervalSec, variationSec);
 *        Automatic organic blinking. E.g. (ON, 3, 2) = blinks every 3-5 seconds.
 *    - eyes.setIdleMode(ON / OFF, intervalSec, variationSec);
 *        Automated eye looking around when idle.
 *
 * E. ONE-SHOT ACTION ANIMATIONS (Self-terminating):
 *    - eyes.anim_laugh(durationMs = 500);       // Vertical giggling shake
 *    - eyes.anim_confused(durationMs = 500);    // Horizontal puzzled shake
 *    - eyes.anim_dizzy(durationMs = 800);       // Multi-axis spiral jitter
 *    - eyes.anim_bounce(durationMs = 600);      // Joyful vertical bounce
 *    - eyes.anim_wink(leftEye = true);          // Winks one eye smoothly
 *    - eyes.anim_sleepy_nod();                  // Droops downward with soft blink
 *    - eyes.blink();                            // Instant manual blink
 *    - eyes.close() / eyes.open();              // Manual eye eyelid control
 */

/*
 * =========================================================================
 * 2. ANATOMY OF AN EMOTION IN EMOTIONENGINE
 * =========================================================================
 * Every emotion preset in EmotionEngine.h follows a clean structure:
 *
 * void setMyCustomEmotion() {
 *     // 1. Update continuous internal state variables (valence & arousal)
 *     state.valence = 0.70f;  // -1.0 (unpleasant) to +1.0 (pleasant)
 *     state.arousal = 0.60f;  //  0.0 (calm/tired) to 1.0 (alert/excited)
 *
 *     // 2. Reset base geometry to clean state
 *     resetEyeGeometry();
 *
 *     // 3. Set emotion mood, curiosity, cyclops, and default decay
 *     setEmotionState(EVA_MY_EMOTION, HAPPY, OFF, OFF, 8000);
 *
 *     // 4. Customize eye geometry (Width, Height, Border Radius, Position)
 *     eyes.setWidth(40, 40);
 *     eyes.setHeight(22, 22);
 *     eyes.setBorderradius(12, 12);
 *     eyes.setPosition(DEFAULT); // or N, S, E, W, etc.
 *
 *     // 5. Optionally trigger micro-animations or flickers
 *     eyes.anim_bounce(500);
 *     // startFlicker(1000); // temporary horizontal flicker for 1s
 *     // startSweat(2000);   // temporary sweat drops for 2s
 *
 *     // 6. Set emotion hold time (how long this shape stays on screen)
 *     emotionHoldUntil = millis() + random(4000, 8000);
 * }
 */

/*
 * =========================================================================
 * 3. TEMPORARY EFFECTS VS PERSISTENT STATES & AUTO-REVERTING
 * =========================================================================
 *
 * Q: How does reverting work? Is it automatic or do I need to revert manually?
 * A: IT IS FULLY AUTOMATIC! Here is how the lifecycle handles it:
 *
 * 1. ONE-SHOT ANIMATIONS (anim_laugh, anim_confused, anim_dizzy, anim_bounce, anim_wink):
 *    - These are non-blocking micro-animations.
 *    - When called (e.g. `eyes.anim_laugh(500)`), they animate for 500ms using `millis()`.
 *    - When the duration expires, the flicker/macro animation automatically turns itself off
 *      and the current eye geometry continues displaying normally.
 *
 * 2. TEMPORARY EFFECTS (startFlicker(ms), startSweat(ms)):
 *    - EmotionEngine manages `updateFlicker()` and `updateSweat()` on every loop tick.
 *    - When the timer expires, `eyes.setHFlicker(OFF)` / `eyes.setSweat(OFF)` is called automatically.
 *
 * 3. EMOTION LIFECYCLE REVERSION (The Companion Flow):
 *    - When `emotion.transitionTo(EVA_EMOTION, holdMs)` is called, EVA smooth-blinks into the emotion.
 *    - The emotion holds for `holdMs` (or the random dwell period).
 *    - BehaviourEngine's lifecycle planner continuously tracks the time. When `holdMs` expires,
 *      BehaviourEngine smoothly picks the NEXT organic emotion according to EVA's internal drives
 *      (Energy, Curiosity, Comfort, Social need).
 *    - You NEVER get stuck in an emotion, and you don't need manual cleanup calls!
 */

/*
 * =========================================================================
 * 4. CATEGORIZING EMOTIONS INTO DRIVE LIFESTYLE BUCKETS
 * =========================================================================
 * EVA's BehaviourEngine divides life into 6 emotional categories:
 *
 * 1. CAT_ENERGETIC (High Energy, High Vitality):
 *    Pool: EVA_EXCITED, EVA_VERY_HAPPY, EVA_AWE, EVA_LOL, EVA_SURPRISED
 *
 * 2. CAT_CONTENT (Warm, Affectionate, Peaceful Social Companion):
 *    Pool: EVA_HAPPY, EVA_AFFECTIONATE, EVA_PROUD, EVA_FOCUSED, EVA_SHY
 *
 * 3. CAT_PENSIVE (Observant, Curious, Thoughtful, Skeptical):
 *    Pool: EVA_CURIOUS, EVA_SKEPTIC, EVA_SUSPICIOUS, EVA_SQUINT, EVA_FOCUSED
 *
 * 4. CAT_MELANCHOLY (Low Energy, Longing, Shy, Bored):
 *    Pool: EVA_SAD, EVA_WORRIED, EVA_UNIMPRESSED, EVA_BORED, EVA_CONFUSED
 *
 * 5. CAT_IRRITABLE (Low Comfort, Annoyed, Frustrated, Grumpy):
 *    Pool: EVA_ANNOYED, EVA_FRUSTRATED, EVA_FURIOUS, EVA_ANGRY
 *
 * 6. CAT_WINDING_DOWN (Exhausted, Pre-Sleep, Heavy Eyelids):
 *    Pool: EVA_BORED, EVA_SLEEPY, EVA_NEUTRAL, EVA_UNIMPRESSED, EVA_SHY
 *
 * TO ADD YOUR EMOTION TO A CATEGORY:
 * In `BehaviourEngine.h` inside `pickEmotionFromCategory()`:
 * Simply add your new `EVA_MY_EMOTION` to the respective array!
 */

/*
 * =========================================================================
 * 5. STEP-BY-STEP RECIPES: DESIGNING NEW EMOTIONS
 * =========================================================================
 */

// Recipe 1: DIZZY / CONFUSED SPIN (Great after spinning or quick turns)
/*
void setDizzy() {
    state.valence = -0.1f; state.arousal = 0.7f;
    resetEyeGeometry();
    setEmotionState(EVA_CONFUSED, DEFAULT, OFF, OFF, 6000);
    eyes.setWidth(34, 34); eyes.setHeight(34, 34); eyes.setBorderradius(12, 12);
    eyes.anim_dizzy(1200); // 1.2s dizzy spiral jitter
    startSweat(2000);      // 2s sweat drops
    emotionHoldUntil = millis() + random(4000, 7000);
}
*/

// Recipe 2: LOVING / ADORING (Great on gentle petting)
/*
void setLove() {
    state.valence = 0.95f; state.arousal = 0.40f; state.trust = 1.0f;
    resetEyeGeometry();
    setEmotionState(EVA_AFFECTIONATE, HAPPY, OFF, OFF, 10000);
    eyes.setWidth(42, 42); eyes.setHeight(16, 16); eyes.setBorderradius(14, 4);
    eyes.anim_bounce(400);
    emotionHoldUntil = millis() + random(5000, 9000);
}
*/

// Recipe 3: PLAYFUL WINK (Cute cheeky trick)
/*
void setWink() {
    state.valence = 0.8f; state.arousal = 0.6f;
    resetEyeGeometry();
    setEmotionState(EVA_HAPPY, HAPPY, OFF, OFF, 6000);
    eyes.setWidth(40, 40); eyes.setHeight(24, 24); eyes.setBorderradius(10, 6);
    eyes.anim_wink(true); // Wink left eye
    emotionHoldUntil = millis() + random(4000, 6000);
}
*/

// Recipe 4: SLEEPY NOD (Restful yawning state)
/*
void setSleepyNod() {
    state.valence = 0.1f; state.arousal = 0.1f;
    resetEyeGeometry();
    setEmotionState(EVA_SLEEPY, TIRED, OFF, OFF, 12000);
    eyes.setWidth(36, 36); eyes.setHeight(10, 10); eyes.setBorderradius(4, 4);
    eyes.anim_sleepy_nod();
    emotionHoldUntil = millis() + random(6000, 10000);
}
*/
