#pragma once
/*
 * BehaviourEngine.h
 * ---------------------------------------------------------
 * EVA's autonomous life engine — rebuilt around a human-like
 * internal drive system and emotional lifecycle planner.
 *
 * Architecture: Three independent layers running in parallel:
 *   [1] REACTIVE LAYER   — obstacle/edge/touch (highest priority)
 *   [2] EMOTION LIFECYCLE — drive-based category planner
 *   [3] ORGANIC MOVEMENT  — gentle, category-weighted motion
 *
 * Internal drives (energy, curiosity, comfort, social) are the
 * "soul" of EVA. They evolve continuously and organically guide
 * which emotion category EVA inhabits, for how long, and what
 * movements feel appropriate. Emotions emerge from EVA's inner
 * state — they are not commanded.
 *
 * Emotion categories, each with a pool of sub-emotions:
 *   CAT_ENERGETIC    — excited, very_happy, awe, happy, curious
 *   CAT_CONTENT      — happy, affectionate, proud, curious, focused
 *   CAT_PENSIVE      — curious, focused, skeptic, suspicious, squint
 *   CAT_MELANCHOLY   — sad, worried, unimpressed, shy, bored
 *   CAT_IRRITABLE    — annoyed, frustrated, confused, angry
 *   CAT_WINDING_DOWN — bored, sleepy, neutral, unimpressed, shy
 *   CAT_SHORT_BURST  — (reactive-only, never scheduled)
 *
 * Duration is dynamic: category hold time and individual emotion
 * hold time are both scaled by the current energy level, so a
 * tired EVA stays in each mood longer and moves less — like a
 * real creature winding down, not a timer counting out.
 *
 * Sleep triggers organically when driveEnergy < ENERGY_SLEEP_THRESHOLD
 * for sleepyThresholdMs — not after a fixed idle timer.
 * ---------------------------------------------------------
 */

#include <Arduino.h>
#include "Types.h"
#include "Config.h"
#include "MovementEngine.h"
#include "MoodLightManager.h"
#include "BuzzerManager.h"
#include "BoredomAnimationManager.h"
#include "CommsHub.h"

template <typename EmotionT>
class BehaviourEngine : public IEmotionRunner {
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
          movementEnabled(true),
          movementSuppressedUntilTouch(false),
          reactiveBusyUntil(0),
          sustainedPetCount(0),
          // --- Internal drives ---
          driveEnergy(0.72f),
          driveCuriosity(0.45f),
          driveComfort(0.80f),
          driveSocial(0.25f),
          lastDrivesUpdateMs(0),
          // --- Emotion lifecycle ---
          activeCategory(CAT_CONTENT),
          categoryEnteredAt(0),
          categoryDurationMs(0),
          nextEmotionAt(0),
          lastCategoryEmotion(EVA_NEUTRAL),
          lastAutoAnimMs(0),
          // --- Sleep system ---
          sleepyRequested(false),
          sleepyThresholdMs(randomSleepyThreshold()),
          lowEnergyTracking(false),
          lowEnergyStartAt(0),
          // --- Motivation spike ---
          nextMotivationCheckAt(0),
          // --- Organic movement ---
          nextOrganicMoveAt(0),
          // --- Cognitive obstacle awareness ---
          lastObstacleTurn(MoveState::PIVOT_LEFT),
          obstacleRepeatStreak(0),
          lastObstacleTime(0) {}

    void setMovementEnabled(bool enabled) { movementEnabled = enabled; }

    void wakeCalm() { movementSuppressedUntilTouch = true; }

    BoredomAnimationManager& getBoredomAnim() { return boredomAnim; }

    void update(SpatialEvent spatial, TouchEvent touch) {
        unsigned long now = millis();

        // Boredom animation takeover (high boredom → run idle animation)
        if (emotion.getBoredom() > 0.8f && !boredomAnim.isPlaying()) {
            IdleAnim pick = (IdleAnim)random(0, 5);
            boredomAnim.trigger(pick);
            emotion.resetBoredom();
        }

        // [LAYER 1] Update internal drives — runs every loop
        updateDrives(now);

        // [LAYER 2] Reactive events (highest priority)
        bool reactiveHandled = handleReactiveEvents(spatial, touch, now);

        // Exit reactive state once cooldown passes
        if (!reactiveHandled && isReactiveState(state)) {
            if (!inReactiveCooldown(now)) {
                state     = BEHAVIOUR_IDLE;
                stateEnteredAt = now;
                lifecycle = EvaLifecycleState::IDLE;
            }
        }

        // [LAYER 3] Emotion lifecycle + motivation spike + sleep pressure
        // (runs alongside but respects reactive cooldown)
        if (!reactiveHandled && !inReactiveCooldown(now)) {
            updateEmotionLifecycle(now);
            checkMotivationSpike(now);
            updateSleepPressure(now);
        }

        // [LAYER 4] Organic movement (independent of emotion timing)
        if (!reactiveHandled && !inReactiveCooldown(now)) {
            updateOrganicMovement(now);
        }

        emotion.update();
        light.update();
        buzzer.update();
        movement.update();
    }

    EvaBehaviour      getState()     const { return state; }
    EvaLifecycleState getLifecycle() const { return lifecycle; }
    EvaMoodState      getMood()      const { return currentMood; }

    bool wantsSleep() const { return sleepyRequested; }

    void clearSleepRequest() {
        sleepyRequested = false;
        // Replenish drives on wake — EVA starts fresh and energetic
        driveEnergy    = ENERGY_WAKE_BOOST;
        driveCuriosity = 0.60f;
        driveComfort   = 0.80f;
        driveSocial    = 0.25f;
        lowEnergyTracking = false;
        lowEnergyStartAt  = 0;
        sleepyThresholdMs = randomSleepyThreshold();
        // Start in an energetic wakeup category
        activeCategory    = CAT_ENERGETIC;
        categoryEnteredAt = millis();
        categoryDurationMs = random(LIFECYCLE_CATEGORY_MIN_MS, LIFECYCLE_CATEGORY_MAX_MS);
        nextEmotionAt     = 0; // pick emotion immediately on next update
        lastDrivesUpdateMs = 0;
        state    = BEHAVIOUR_IDLE;
        lifecycle = EvaLifecycleState::IDLE;
    }

    void setLifecycle(EvaLifecycleState nextLifecycle) { lifecycle = nextLifecycle; }

    // =====================================================================
    // BLE Action Triggers (IEmotionRunner)
    // These are "sticky" — lifecycle planner is pushed forward so BLE
    // emotions have time to breathe before EVA's inner world takes over again.
    // =====================================================================

    void triggerEmotion(EvaEmotion emo) override {
        const unsigned long BLE_HOLD = 20000UL;
        emotion.transitionTo(emo, BLE_HOLD);
        syncMood(emo);
        light.showEmotion(emo);
        buzzer.playForEmotion(emo);
        nextEmotionAt = millis() + BLE_HOLD + 5000UL;
        if (canMove()) {
            if (emo == EVA_HAPPY || emo == EVA_VERY_HAPPY || emo == EVA_EXCITED)
                movement.driveFor(MoveState::WIGGLE, 250, 130);
            else if (emo == EVA_LOL || emo == EVA_PROUD)
                movement.driveFor(MoveState::DONUT, 320, 160);
            else if (emo == EVA_SCARED || emo == EVA_STARTLED)
                movement.driveFor(MoveState::FLINCH_BACK, 200, 180);
        }
    }

    void triggerSpin() override {
        emotion.transitionTo(EVA_VERY_HAPPY, 12000UL);
        syncMood(EVA_VERY_HAPPY);
        light.showEmotion(EVA_VERY_HAPPY);
        if (!boredomAnim.isPlaying()) buzzer.playForEmotion(EVA_VERY_HAPPY);
        nextEmotionAt = millis() + 16000UL;
        if (canMove()) movement.driveFor(MoveState::DONUT, 1500, 220);
    }

    void triggerTrick() override {
        emotion.transitionTo(EVA_LOL, 12000UL);
        syncMood(EVA_LOL);
        light.showEmotion(EVA_LOL);
        if (!boredomAnim.isPlaying()) buzzer.playForEmotion(EVA_LOL);
        nextEmotionAt = millis() + 16000UL;
        if (canMove()) movement.driveFor(MoveState::DONUT, 1500, 210);
    }

    void triggerDance() override {
        emotion.transitionTo(EVA_VERY_HAPPY, 20000UL);
        syncMood(EVA_VERY_HAPPY);
        light.showEmotion(EVA_VERY_HAPPY);
        if (!boredomAnim.isPlaying()) buzzer.playChirpUp();
        nextEmotionAt = millis() + 24000UL;
        if (canMove()) {
            movement.startDanceRoutine();
        }
    }

    void triggerScare() override {
        emotion.setStartled();
        syncMood(EVA_SCARED);
        light.showEmotion(EVA_STARTLED);
        if (!boredomAnim.isPlaying()) buzzer.playForEmotion(EVA_STARTLED);
        nextEmotionAt = millis() + 10000UL;
        if (canMove()) movement.driveFor(MoveState::FLINCH_BACK, 250, 220);
    }

    void triggerRandom() override {
        static const EvaEmotion ems[] = {
            EVA_HAPPY, EVA_VERY_HAPPY, EVA_CURIOUS, EVA_LOL, EVA_EXCITED,
            EVA_CONFUSED, EVA_PROUD, EVA_SUSPICIOUS, EVA_AFFECTIONATE, EVA_SHY,
            EVA_ANNOYED, EVA_SCARED, EVA_BORED, EVA_STARTLED, EVA_SAD,
            EVA_SKEPTIC, EVA_WORRIED, EVA_FOCUSED, EVA_SURPRISED, EVA_FRUSTRATED,
            EVA_UNIMPRESSED, EVA_SQUINT, EVA_FURIOUS, EVA_AWE
        };
        triggerEmotion(ems[random(0, 24)]);
    }

    void triggerAnimation(uint8_t animId) override {
        if (animId < 5) boredomAnim.trigger((IdleAnim)animId, random(10000UL, 30000UL));
    }

    void setAnimationsEnabled(bool en) override { boredomAnim.setEnabled(en); }
    void startDinoGame()               override { boredomAnim.startDinoGame(); }
    void jumpDino()                    override { boredomAnim.jumpDino(); }

private:
    EmotionT          &emotion;
    MovementEngine    &movement;
    MoodLightManager  &light;
    BuzzerManager     &buzzer;
    BoredomAnimationManager boredomAnim;

    // -- Behaviour state --
    EvaBehaviour      state;
    EvaLifecycleState lifecycle;
    EvaMoodState      currentMood;
    unsigned long     stateEnteredAt;
    bool              movementEnabled;
    bool              movementSuppressedUntilTouch;
    unsigned long     reactiveBusyUntil;
    uint8_t           sustainedPetCount;

    // =====================================================================
    // INTERNAL DRIVES — the "soul" of EVA
    // These values evolve every loop() tick and guide everything else.
    // =====================================================================
    float         driveEnergy;      // 0-1: vitality; depletes → sleep
    float         driveCuriosity;   // 0-1: rises idle, drops on movement
    float         driveComfort;     // 0-1: drops on bad events, restores slowly
    float         driveSocial;      // 0-1: rises when alone, drops on touch
    unsigned long lastDrivesUpdateMs;

    // =====================================================================
    // EMOTION LIFECYCLE — drive-based category + sub-emotion planner
    // =====================================================================
    EmotionCategory activeCategory;
    unsigned long   categoryEnteredAt;
    unsigned long   categoryDurationMs;   // dynamic, energy-scaled
    unsigned long   nextEmotionAt;        // when to pick next sub-emotion
    EvaEmotion      lastCategoryEmotion;  // prevent immediate repeat
    unsigned long   lastAutoAnimMs;

    // =====================================================================
    // SLEEP SYSTEM (drive-based, not timer-based)
    // =====================================================================
    bool          sleepyRequested;
    unsigned long sleepyThresholdMs;  // how long energy must stay low
    bool          lowEnergyTracking;
    unsigned long lowEnergyStartAt;

    // =====================================================================
    // MOTIVATION SPIKE
    // =====================================================================
    unsigned long nextMotivationCheckAt;

    // =====================================================================
    // ORGANIC MOVEMENT
    // =====================================================================
    unsigned long nextOrganicMoveAt;

    // =====================================================================
    // COGNITIVE OBSTACLE AWARENESS
    // =====================================================================
    MoveState     lastObstacleTurn;
    uint8_t       obstacleRepeatStreak;
    unsigned long lastObstacleTime;

    // =====================================================================
    // HELPERS
    // =====================================================================
    static bool isReactiveState(EvaBehaviour s) {
        return s == BEHAVIOUR_OBSTACLE_AVOID || s == BEHAVIOUR_EDGE_AVOID  ||
               s == BEHAVIOUR_TOUCH_REACT    ||
               s == BEHAVIOUR_IDLE_FLOURISH;
    }

    static unsigned long randomSleepyThreshold() {
        return random(BEHAVIOUR_SLEEPY_AFTER_MIN_MS, BEHAVIOUR_SLEEPY_AFTER_MAX_MS);
    }
    static unsigned long randomReactionDwell() {
        return random(BEHAVIOUR_REACTION_DWELL_MIN_MS, BEHAVIOUR_REACTION_DWELL_MAX_MS);
    }

    bool canMove() const { return movementEnabled && !movementSuppressedUntilTouch; }
    bool inReactiveCooldown(unsigned long now) const { return now < reactiveBusyUntil; }

    void syncMood(EvaEmotion em) {
        EvaMoodState next = moodForEmotion(em);
        if (currentMood != next) currentMood = next;
    }

    static EvaMoodState moodForEmotion(EvaEmotion em) {
        switch (em) {
            case EVA_HAPPY:       return EvaMoodState::MOOD_HAPPY;
            case EVA_VERY_HAPPY:  return EvaMoodState::MOOD_EXCITED;
            case EVA_CURIOUS:     return EvaMoodState::MOOD_CURIOUS;
            case EVA_SCARED:      return EvaMoodState::MOOD_SCARED;
            case EVA_ANGRY:       return EvaMoodState::MOOD_ANGRY;
            case EVA_SLEEPY:      return EvaMoodState::MOOD_SLEEPY;
            case EVA_NEUTRAL:     return EvaMoodState::MOOD_CALM;
            case EVA_LOL:         return EvaMoodState::MOOD_EXCITED;
            case EVA_ANNOYED:     return EvaMoodState::MOOD_ANGRY;
            case EVA_AFFECTIONATE:return EvaMoodState::MOOD_HAPPY;
            case EVA_EXCITED:     return EvaMoodState::MOOD_EXCITED;
            case EVA_PROUD:       return EvaMoodState::MOOD_HAPPY;
            case EVA_SKEPTIC:     return EvaMoodState::MOOD_CURIOUS;
            case EVA_WORRIED:     return EvaMoodState::MOOD_SCARED;
            case EVA_FOCUSED:     return EvaMoodState::MOOD_CALM;
            case EVA_SURPRISED:   return EvaMoodState::MOOD_EXCITED;
            case EVA_FRUSTRATED:  return EvaMoodState::MOOD_ANGRY;
            case EVA_UNIMPRESSED: return EvaMoodState::MOOD_CALM;
            case EVA_SQUINT:      return EvaMoodState::MOOD_CURIOUS;
            case EVA_FURIOUS:     return EvaMoodState::MOOD_ANGRY;
            case EVA_AWE:         return EvaMoodState::MOOD_HAPPY;
            default:              return EvaMoodState::MOOD_CALM;
        }
    }

    void finishReaction(EvaBehaviour reaction, unsigned long now, unsigned long busyMs) {
        state          = reaction;
        stateEnteredAt = now;
        reactiveBusyUntil = now + busyMs;
        // Push emotion planner forward — let the reaction breathe naturally.
        // The lifecycle will resume after the reaction + a natural linger period.
        unsigned long lingerMs = busyMs + random(12000UL, 28000UL);
        if (now + lingerMs > nextEmotionAt) nextEmotionAt = now + lingerMs;
        // Give organic movement a rest after reactions
        unsigned long moveRestMs = busyMs + random(3000UL, 8000UL);
        if (now + moveRestMs > nextOrganicMoveAt) nextOrganicMoveAt = now + moveRestMs;
    }

    // =====================================================================
    // [LAYER 1] DRIVES UPDATE
    // Runs every loop() — the heartbeat of EVA's inner world
    // =====================================================================
    void updateDrives(unsigned long now) {
        if (lastDrivesUpdateMs == 0) { lastDrivesUpdateMs = now; return; }
        float dt = (now - lastDrivesUpdateMs) / 1000.0f;
        if (dt > 2.0f) dt = 0.10f;  // safety cap (e.g. waking from blocking call)
        lastDrivesUpdateMs = now;

        // Energy drains steadily while awake — the core sleep driver
        driveEnergy -= ENERGY_DECAY_RATE_PER_S * dt;
        driveEnergy  = max(0.0f, driveEnergy);

        // Curiosity: rises when still (boredom), satisfied by movement
        if (!movement.isMoving()) {
            driveCuriosity += CURIOSITY_RISE_RATE_PER_S * dt;
        } else {
            driveCuriosity -= CURIOSITY_SATISFY_RATE_PER_S * dt;
        }
        driveCuriosity = constrain(driveCuriosity, 0.0f, 1.0f);

        // Comfort drifts toward homeostasis (0.7) slowly
        driveComfort += (0.70f - driveComfort) * COMFORT_RESTORE_RATE * dt;
        driveComfort  = constrain(driveComfort, 0.0f, 1.0f);

        // Social need builds when alone
        driveSocial += SOCIAL_RISE_RATE_PER_S * dt;
        driveSocial  = constrain(driveSocial, 0.0f, 1.0f);
    }

    // =====================================================================
    // [LAYER 2] EMOTION LIFECYCLE PLANNER
    // Two-level: category block (minutes) → sub-emotion (seconds to minutes)
    // =====================================================================
    void updateEmotionLifecycle(unsigned long now) {
        // First-time initialisation
        if (categoryEnteredAt == 0) {
            pickNewCategory(now);
            return;
        }
        // Category block expired → pick new category
        if (now - categoryEnteredAt >= categoryDurationMs) {
            pickNewCategory(now);
            return;
        }
        // Sub-emotion hold expired → pick next within category
        if (now >= nextEmotionAt && !emotion.isEmotionHeld()) {
            pickEmotionFromCategory(now);
        }
    }

    // Select a new emotion category and start the block
    void pickNewCategory(unsigned long now) {
        EmotionCategory newCat = selectCategoryFromDrives();

        // Avoid pure immediate repeat (65% chance to re-roll)
        if (newCat == activeCategory && random(0, 100) < 65) {
            newCat = selectCategoryFromDrives();
        }

        activeCategory    = newCat;
        categoryEnteredAt = now;

        // Dynamic duration: energy-scaled (tired → longer, more lethargic stays)
        unsigned long base = random(LIFECYCLE_CATEGORY_MIN_MS, LIFECYCLE_CATEGORY_MAX_MS);
        float energyScale  = 0.55f + driveEnergy * 0.90f;  // 0.55x–1.45x
        categoryDurationMs = (unsigned long)(base * energyScale);

        static const char* CAT_NAMES[] = {"ENERGETIC","CONTENT","PENSIVE","MELANCHOLY","IRRITABLE","WINDING_DOWN","SHORT_BURST"};
        EVA_LOGF("[EVA] Category→%-12s  dur=%lus  E=%.2f C=%.2f Cmf=%.2f S=%.2f\n",
                 CAT_NAMES[(int)activeCategory], categoryDurationMs/1000UL,
                 driveEnergy, driveCuriosity, driveComfort, driveSocial);

        nextEmotionAt = 0;  // pick sub-emotion immediately
    }

    // Weighted category selection driven by current internal drives.
    // Energy is the primary governor; curiosity/comfort/social refine.
    EmotionCategory selectCategoryFromDrives() {
        // Very low energy → winding down toward sleep
        if (driveEnergy < 0.15f)
            return CAT_WINDING_DOWN;

        // Low energy → mostly melancholy, occasionally winding down
        if (driveEnergy < 0.28f)
            return (random(0, 100) < 65) ? CAT_MELANCHOLY : CAT_WINDING_DOWN;

        // Low comfort → irritable (discomfort breeds frustration)
        if (driveComfort < 0.25f && random(0, 100) < 55)
            return CAT_IRRITABLE;

        // High social need + decent energy → content/affectionate
        if (driveSocial > 0.70f && driveEnergy > 0.40f && random(0, 100) < 50)
            return CAT_CONTENT;

        // High energy + high curiosity → energetic exploration
        if (driveEnergy > 0.65f && driveCuriosity > 0.50f)
            return (random(0, 100) < 60) ? CAT_ENERGETIC : CAT_CONTENT;

        // High energy, lower curiosity → content and settled
        if (driveEnergy > 0.55f) {
            int r = random(0, 100);
            if (r < 40) return CAT_CONTENT;
            if (r < 65) return CAT_ENERGETIC;
            if (r < 85) return CAT_PENSIVE;
            return CAT_MELANCHOLY;
        }

        // Medium energy (0.28–0.55) — thoughtful, calm, or a bit low
        {
            int r = random(0, 100);
            if (r < 32) return CAT_PENSIVE;
            if (r < 58) return CAT_CONTENT;
            if (r < 80) return CAT_MELANCHOLY;
            return (driveComfort < 0.40f) ? CAT_IRRITABLE : CAT_PENSIVE;
        }
    }

    // Pick and apply the next sub-emotion within the active category.
    // Each category has its own distinct emotion pool — no emotion
    // is "primary" in multiple pools so mood feels clearly different.
    void pickEmotionFromCategory(unsigned long now) {
        // OG emotion list kept intact. Pools are distinct:
        // ENERGETIC   → the "peak" positive states (awe, excitement)
        // CONTENT     → warm & social (affection, pride, happiness)
        // PENSIVE     → thoughtful / observant (no overlap with above)
        // MELANCHOLY  → the low states
        // IRRITABLE   → the friction states
        // WINDING_DOWN→ the pre-sleep wind-down states
        static const EvaEmotion ENERGETIC_EMO[] = { EVA_EXCITED, EVA_VERY_HAPPY, EVA_AWE, EVA_LOL, EVA_SURPRISED };
        static const EvaEmotion CONTENT_EMO[]   = { EVA_HAPPY, EVA_AFFECTIONATE, EVA_PROUD, EVA_FOCUSED, EVA_SHY };
        static const EvaEmotion PENSIVE_EMO[]   = { EVA_CURIOUS, EVA_SKEPTIC, EVA_SUSPICIOUS, EVA_SQUINT, EVA_FOCUSED };
        static const EvaEmotion MELANCHOLY_EMO[]= { EVA_SAD, EVA_WORRIED, EVA_UNIMPRESSED, EVA_BORED, EVA_CONFUSED };
        static const EvaEmotion IRRITABLE_EMO[] = { EVA_ANNOYED, EVA_FRUSTRATED, EVA_FURIOUS, EVA_ANGRY };
        static const EvaEmotion WINDING_EMO[]   = { EVA_BORED, EVA_SLEEPY, EVA_NEUTRAL, EVA_UNIMPRESSED, EVA_SHY };

        const EvaEmotion *pool = CONTENT_EMO;
        uint8_t count = 5;
        switch (activeCategory) {
            case CAT_ENERGETIC:    pool = ENERGETIC_EMO; count = 5; break;
            case CAT_CONTENT:      pool = CONTENT_EMO;   count = 5; break;
            case CAT_PENSIVE:      pool = PENSIVE_EMO;   count = 5; break;
            case CAT_MELANCHOLY:   pool = MELANCHOLY_EMO;count = 5; break;
            case CAT_IRRITABLE:    pool = IRRITABLE_EMO; count = 4; break;
            case CAT_WINDING_DOWN: pool = WINDING_EMO;   count = 5; break;
            default: break;
        }

        // Pick, avoiding immediate repeat (up to 3 tries)
        EvaEmotion picked = pool[random(0, count)];
        for (uint8_t t = 0; picked == lastCategoryEmotion && t < 3; t++) {
            picked = pool[random(0, count)];
        }
        lastCategoryEmotion = picked;

        // Hold time: base range × energy modifier
        // Low energy = more lethargic → longer holds (0.7x to 1.3x scale)
        unsigned long holdBase;
        switch (activeCategory) {
            case CAT_ENERGETIC:    holdBase = random(18000UL, 38000UL); break;
            case CAT_CONTENT:      holdBase = random(25000UL, 55000UL); break;
            case CAT_PENSIVE:      holdBase = random(30000UL, 68000UL); break;
            case CAT_MELANCHOLY:   holdBase = random(40000UL, 85000UL); break;
            case CAT_IRRITABLE:    holdBase = random(18000UL, 42000UL); break;
            case CAT_WINDING_DOWN: holdBase = random(50000UL, 110000UL); break;
            default:               holdBase = 30000UL; break;
        }
        float   energyMod = 0.70f + (1.0f - driveEnergy) * 0.60f;
        unsigned long holdMs = (unsigned long)(holdBase * energyMod);

        emotion.transitionTo(picked, holdMs);
        syncMood(picked);
        light.showEmotion(picked);
        if (!boredomAnim.isPlaying()) buzzer.playForEmotion(picked);

        nextEmotionAt = now + holdMs;

        // Print emotion pick for verification
        // OG EvaEmotion enum order (0-based, matches Types.h exactly):
        static const char* EMO_NAMES[] = {
            "NEUTRAL","HAPPY","VERY_HAPPY","CURIOUS","SCARED","LOL","ANGRY","SLEEPY",
            "ANNOYED","AFFECTIONATE","SHY","EXCITED","STARTLED","CONFUSED","BORED",
            "PROUD","SUSPICIOUS","SAD","SKEPTIC","WORRIED","FOCUSED","SURPRISED",
            "FRUSTRATED","UNIMPRESSED","SQUINT","FURIOUS","AWE"
        };
        if ((int)picked < 27) {
            EVA_LOGF("[EVA] Emotion→%-12s  hold=%lus  E=%.2f\n",
                     EMO_NAMES[(int)picked], holdMs/1000UL, driveEnergy);
        }

        // Occasional takeover animation in introspective/low-energy states
        // (min 60s gap, 18% chance, only when not already playing)
        if ((activeCategory == CAT_PENSIVE    ||
             activeCategory == CAT_MELANCHOLY ||
             activeCategory == CAT_WINDING_DOWN) &&
            now - lastAutoAnimMs >= 60000UL   &&
            random(0, 100) < 18               &&
            !boredomAnim.isPlaying()) {
            lastAutoAnimMs = now;
            static const IdleAnim ANIMS[] = { ANIM_DINO, ANIM_PULLUPS, ANIM_SNOW, ANIM_CAR };
            IdleAnim chosen = ANIMS[random(0, 4)];
            boredomAnim.trigger(chosen, random(10000UL, 25000UL));
            static const char* ANIM_NAMES[] = {"DINO","PULLUPS","MUSIC","SNOW","CAR"};
            EVA_LOGF("[EVA] Anim→%s  source=AUTO (low-energy/pensive)\n",
                     ANIM_NAMES[(int)chosen]);
        }
    }

    // =====================================================================
    // MOTIVATION SPIKE
    // Simulates the human "second wind" — a sudden burst of enthusiasm
    // that can lift EVA out of a low mood temporarily.
    // =====================================================================
    void checkMotivationSpike(unsigned long now) {
        if (now < nextMotivationCheckAt) return;
        nextMotivationCheckAt = now + MOTIVATION_SPIKE_CHECK_MS;

        // Probability per check: highest in the "second wind" mid-energy window,
        // near-zero when exhausted, lower when already high-energy.
        int ppt = 0; // per-thousand probability
        if      (driveEnergy < 0.15f) ppt = 0;
        else if (driveEnergy < 0.30f) ppt = 15;  // rare spark before sleep
        else if (driveEnergy < 0.55f) ppt = 50;  // classic "second wind" zone
        else if (driveEnergy < 0.75f) ppt = 25;  // still active, smaller boost
        else                          ppt = 10;  // already energetic

        if (ppt > 0 && random(0, 1000) < ppt) {
            triggerMotivationSpike(now);
        }
    }

    void triggerMotivationSpike(unsigned long now) {
        // Boost drives with a random amount of enthusiasm
        driveEnergy    = min(1.0f, driveEnergy + (random(15, 32) / 100.0f));
        driveCuriosity = min(1.0f, driveCuriosity + 0.20f);

        // Shift to a positive category
        activeCategory     = (random(0, 2) == 0) ? CAT_ENERGETIC : CAT_CONTENT;
        categoryEnteredAt  = now;
        categoryDurationMs = random(50000UL, 120000UL);

        // Pick a joyful "spark" emotion with a decent hold
        static const EvaEmotion SPIKE_EMO[] = { EVA_HAPPY, EVA_EXCITED, EVA_VERY_HAPPY, EVA_AWE, EVA_LOL };
        EvaEmotion picked = SPIKE_EMO[random(0, 5)];
        unsigned long holdMs = random(22000UL, 42000UL);

        emotion.transitionTo(picked, holdMs);
        syncMood(picked);
        light.showEmotion(picked);
        if (!boredomAnim.isPlaying()) buzzer.playChirpUp();
        nextEmotionAt = now + holdMs;

        EVA_LOGF("[EVA] *** MOTIVATION SPIKE! E=%.2f→%.2f  cat=%s\n",
                 driveEnergy - (random(15,32)/100.0f), driveEnergy,
                 (activeCategory == CAT_ENERGETIC) ? "ENERGETIC" : "CONTENT");

        // Small celebratory movement
        if (canMove()) {
            static const MoveState SPIKE_MOVES[] = {
                MoveState::WIGGLE, MoveState::HOP_FORWARD,
                MoveState::PIVOT_LEFT, MoveState::PIVOT_RIGHT
            };
            movement.driveFor(SPIKE_MOVES[random(0, 4)], random(250, 400), random(130, 165));
        }
    }

    // =====================================================================
    // SLEEP PRESSURE (drive-based)
    // Energy below threshold for long enough → request sleep organically
    // =====================================================================
    void updateSleepPressure(unsigned long now) {
        if (driveEnergy < ENERGY_SLEEP_THRESHOLD) {
            if (!lowEnergyTracking) {
                lowEnergyTracking = true;
                lowEnergyStartAt  = now;
                EVA_LOGF("[EVA] Low energy (%.2f) — sleep countdown started (%lus)\n",
                         driveEnergy, sleepyThresholdMs/1000UL);
                // Shift to winding-down visuals while energy is low
                if (activeCategory != CAT_WINDING_DOWN) {
                    activeCategory     = CAT_WINDING_DOWN;
                    categoryEnteredAt  = now;
                    categoryDurationMs = sleepyThresholdMs + 8000UL;
                    nextEmotionAt      = 0; // pick WINDING emotion immediately
                }
            } else if (now - lowEnergyStartAt >= sleepyThresholdMs) {
                EVA_LOGLN("[EVA] >>> SLEEP REQUESTED (energy exhausted) <<<");
                emotion.transitionTo(EVA_SLEEPY, 8000UL);
                syncMood(EVA_SLEEPY);
                light.showEmotion(EVA_SLEEPY);
                state     = BEHAVIOUR_SLEEPY;
                lifecycle = EvaLifecycleState::SLEEPING;
                sleepyRequested = true;
            }
        } else {
            // Energy recovered (e.g. touch boost) — cancel tracking
            if (lowEnergyTracking) {
                lowEnergyTracking = false;
                lowEnergyStartAt  = 0;
                EVA_LOGLN("[EVA] Energy recovered — sleep countdown cancelled");
            }
        }
    }

    // =====================================================================
    // [LAYER 3] ORGANIC MOVEMENT
    // Independent of emotion timing. Category determines movement style;
    // 90%+ of moves are slow and gentle — matching real companion robots.
    // =====================================================================
    void updateOrganicMovement(unsigned long now) {
        if (!canMove())         return;
        if (movement.isMoving()) return;
        if (now < nextOrganicMoveAt) return;

        // Chance to move at all — low energy = mostly still
        uint8_t moveChancePct;
        switch (activeCategory) {
            case CAT_ENERGETIC:    moveChancePct = 68; break;
            case CAT_CONTENT:      moveChancePct = 48; break;
            case CAT_PENSIVE:      moveChancePct = 28; break;
            case CAT_MELANCHOLY:   moveChancePct = 18; break;
            case CAT_IRRITABLE:    moveChancePct = 38; break;
            case CAT_WINDING_DOWN: moveChancePct =  8; break;
            default:               moveChancePct = 30; break;
        }

        // Gap before next decision: longer when tired (lethargy)
        unsigned long baseGap = random(3000UL, 11000UL);
        float tiredness = 0.50f + (1.0f - driveEnergy) * 1.50f; // 0.5x–2.0x
        nextOrganicMoveAt = now + (unsigned long)(baseGap * tiredness);

        if (random(0, 100) >= moveChancePct) return; // Stay still / daydreaming

        // Pick and execute movement based on active category
        MoveState     move = MoveState::STOP;
        uint8_t       spd  = 0;
        unsigned long dur  = 0;

        switch (activeCategory) {
            case CAT_ENERGETIC:    pickEnergeticMove(move, spd, dur); break;
            case CAT_CONTENT:      pickContentMove(move, spd, dur);   break;
            case CAT_PENSIVE:      pickPensiveMove(move, spd, dur);   break;
            case CAT_MELANCHOLY:   pickMelancholyMove(move, spd, dur);break;
            case CAT_IRRITABLE:    pickIrritableMove(move, spd, dur); break;
            case CAT_WINDING_DOWN: pickWindingDownMove(move, spd, dur);break;
            default:               pickContentMove(move, spd, dur);   break;
        }

        if (dur > 0) movement.driveFor(move, dur, spd);
    }

    // -- Movement palettes (low speed is the default; bursts are rare) --

    void pickEnergeticMove(MoveState &m, uint8_t &s, unsigned long &d) {
        // 30% slow-fwd, 25% slow-curve, 13% pivot, 12% hop, 12% wiggle, 8% donut (rare!)
        int r = random(0, 100);
        if      (r < 30) { m = MoveState::FORWARD;       s = random(130,160); d = random(300,600); }
        else if (r < 55) { m = MoveState::SLOW_CURVE_FWD;s = random(115,140); d = random(400,700); }
        else if (r < 68) { m = (random(0,2)==0) ? MoveState::PIVOT_LEFT : MoveState::PIVOT_RIGHT;
                           s = random(130,155); d = random(250,450); }
        else if (r < 80) { m = MoveState::HOP_FORWARD;   s = random(148,175); d = 220; }
        else if (r < 92) { m = MoveState::WIGGLE;         s = random(138,165); d = random(240,370); }
        else             { m = MoveState::DONUT;           s = random(160,195); d = random(400,650); }
    }

    void pickContentMove(MoveState &m, uint8_t &s, unsigned long &d) {
        // Mostly slow & smooth — no fast moves in content mode
        int r = random(0, 100);
        if      (r < 35) { m = MoveState::SLOW_FORWARD;   s = random(90,118); d = random(400,800); }
        else if (r < 62) { m = MoveState::SLOW_CURVE_FWD; s = random(90,112); d = random(500,900); }
        else if (r < 78) { m = (random(0,2)==0) ? MoveState::PIVOT_LEFT : MoveState::PIVOT_RIGHT;
                           s = random(100,125); d = random(300,500); }
        else if (r < 90) { m = MoveState::HOP_FORWARD;    s = random(128,150); d = 200; }
        else             { m = MoveState::WIGGLE;           s = random(118,140); d = random(200,320); }
    }

    void pickPensiveMove(MoveState &m, uint8_t &s, unsigned long &d) {
        // Slow pivots (looking around / thinking) + occasional gentle drift
        int r = random(0, 100);
        if      (r < 40) { m = (random(0,2)==0) ? MoveState::PIVOT_LEFT : MoveState::PIVOT_RIGHT;
                           s = random(78,102); d = random(420,720); }
        else if (r < 66) { m = MoveState::SLOW_FORWARD;    s = random(75,96);  d = random(400,700); }
        else if (r < 83) { m = MoveState::BACKWARD;         s = random(72,92);  d = random(250,430); }
        else             { m = MoveState::SLOW_CURVE_FWD;   s = random(75,95);  d = random(500,800); }
    }

    void pickMelancholyMove(MoveState &m, uint8_t &s, unsigned long &d) {
        // Very slow, lethargic — barely moving
        int r = random(0, 100);
        if      (r < 40) { m = MoveState::SLOW_FORWARD;    s = random(65,85);  d = random(280,560); }
        else if (r < 70) { m = (random(0,2)==0) ? MoveState::PIVOT_LEFT : MoveState::PIVOT_RIGHT;
                           s = random(65,85);  d = random(340,600); }
        else             { m = MoveState::BACKWARD;          s = random(62,82);  d = random(240,400); }
    }

    void pickIrritableMove(MoveState &m, uint8_t &s, unsigned long &d) {
        // Twitchy: quick short bursts of tension — but doesn't travel far
        int r = random(0, 100);
        if      (r < 32) { m = (random(0,2)==0) ? MoveState::PIVOT_LEFT : MoveState::PIVOT_RIGHT;
                           s = random(148,175); d = random(150,280); }
        else if (r < 52) { m = MoveState::FORWARD;          s = random(138,165); d = random(200,340); }
        else if (r < 68) { m = MoveState::JERK_STEPS;       s = random(155,185); d = random(150,260); }
        else if (r < 84) { m = MoveState::WIGGLE;            s = random(148,175); d = random(200,320); }
        else             { m = MoveState::BACKWARD;           s = random(128,155); d = random(200,330); }
    }

    void pickWindingDownMove(MoveState &m, uint8_t &s, unsigned long &d) {
        // Almost no movement — barely drifting, nearly asleep
        m = (random(0, 3) == 0) ? MoveState::SLOW_FORWARD :
            (random(0, 2) == 0) ? MoveState::PIVOT_LEFT : MoveState::PIVOT_RIGHT;
        s = random(60, 80);
        d = random(270, 490);
    }

    // =====================================================================
    // [LAYER 1] REACTIVE EVENTS
    // Highest priority — safety, touch, and sound reactions.
    // Each reaction also updates the relevant drives contextually.
    // =====================================================================
    bool handleReactiveEvents(SpatialEvent spatial, TouchEvent touch,
                               unsigned long now) {
        if (inReactiveCooldown(now)) return true;

        if (touch != TouchEvent::NONE) {
            movementSuppressedUntilTouch = false;
            if (boredomAnim.isGameActive()) { boredomAnim.jumpDino(); return true; }
        }

        if (canMove() && spatial == SpatialEvent::OBSTACLE) { reactToObstacle(now); return true; }
        if (canMove() && spatial == SpatialEvent::EDGE)     { reactToEdge(now);     return true; }
        if (touch == TouchEvent::TAP)        { reactToTap(now);       return true; }
        if (touch == TouchEvent::DOUBLE_TAP) { reactToDoubleTap(now); return true; }
        if (touch == TouchEvent::PETTING)    { reactToPetting(now);   return true; }
        if (touch == TouchEvent::LONG_HOLD)  { reactToLongHold(now);  return true; }

        return false;
    }

    void reactToObstacle(unsigned long now) {
        movement.stop();

        // Check if this obstacle was detected immediately after a previous evasion maneuver (closing in)
        bool isRepeatedOrClosingIn = (now - lastObstacleTime < 3500UL);
        lastObstacleTime = now;

        if (isRepeatedOrClosingIn) {
            obstacleRepeatStreak++;
        } else {
            obstacleRepeatStreak = 1;
        }

        // Gentle comfort drop to avoid dropping straight to 0.00
        driveComfort = max(0.20f, driveComfort - 0.08f);

        if (obstacleRepeatStreak >= 2) {
            // Human-like cognitive response:
            // "Uh oh, I'm still too close / turning into it!"
            EvaEmotion reactionEmo = (random(0, 2) == 0) ? EVA_CONFUSED : EVA_SKEPTIC;
            emotion.transitionTo(reactionEmo, 4000UL);
            syncMood(reactionEmo);
            light.showEmotion(reactionEmo);
            buzzer.playForEmotion(reactionEmo);

            // Reverse course: hard reverse curve and flip direction with full 120° breakaway pivot
            MoveState oppositeTurn = (lastObstacleTurn == MoveState::PIVOT_LEFT) ? MoveState::PIVOT_RIGHT : MoveState::PIVOT_LEFT;
            lastObstacleTurn = oppositeTurn;
            obstacleRepeatStreak = 0; // reset after rethink

            uint8_t revSpeed = random(160, 195);
            unsigned long revDur = random(750, 1100);
            movement.driveFor(oppositeTurn, revDur, revSpeed);
            EVA_LOGLN("[EVA] Obstacle→COGNITIVE RETHINK (Backing up & flipping direction 120°)");
            finishReaction(BEHAVIOUR_OBSTACLE_AVOID, now, revDur + MOVE_STOP_SETTLE_MS);
        } else {
            // First obstacle encounter — curious / alert pivot (wide 80°-100° rotation)
            emotion.transitionTo(EVA_CURIOUS, 3500UL);
            syncMood(EVA_CURIOUS);
            light.showEmotion(EVA_CURIOUS);
            buzzer.playCuriousHmm();

            MoveState turn = (random(0, 2) == 0) ? MoveState::PIVOT_LEFT : MoveState::PIVOT_RIGHT;
            lastObstacleTurn = turn;
            uint8_t turnSpeed = random(175, 205);
            unsigned long turnDur = random(650, 950);

            movement.driveFor(turn, turnDur, turnSpeed);
            EVA_LOGF("[EVA] Obstacle→AVOID (Turn %s, spd=%u, dur=%lums)\n",
                     (turn == MoveState::PIVOT_LEFT) ? "LEFT" : "RIGHT", turnSpeed, turnDur);
            finishReaction(BEHAVIOUR_OBSTACLE_AVOID, now, turnDur + MOVE_STOP_SETTLE_MS);
        }
    }

    void reactToEdge(unsigned long now) {
        movement.stop();
        driveComfort = max(0.20f, driveComfort - 0.10f);

        // Edge avoidance: MUST reverse AND pivot away so ToF sensor is safely rotated off the edge!
        int choice = random(0, 100);
        if (choice < 50) {
            // Maneuver 1: Startled reflex reverse + pivot turn away
            emotion.setStartled();
            syncMood(EVA_SCARED);
            light.showEmotion(EVA_STARTLED);
            buzzer.playStartled();
            
            uint8_t curveSpd = random(160, 195);
            unsigned long curveDur = random(700, 1000);
            movement.driveFor(MoveState::SLOW_CURVE_REV, curveDur, curveSpd);
            EVA_LOGLN("[EVA] Edge→FLINCH_AND_CURVE_BACK (Startled retreat & pivot away)");
            finishReaction(BEHAVIOUR_EDGE_AVOID, now, curveDur + MOVE_STOP_SETTLE_MS);
        } else {
            // Maneuver 2: Cautious reverse pivot away from edge
            emotion.transitionTo(EVA_WORRIED, 5000UL);
            syncMood(EVA_SCARED);
            light.showEmotion(EVA_WORRIED);
            buzzer.playForEmotion(EVA_WORRIED);

            MoveState pivotAway = (random(0, 2) == 0) ? MoveState::PIVOT_LEFT : MoveState::PIVOT_RIGHT;
            uint8_t turnSpd = random(175, 205);
            unsigned long turnDur = random(700, 950);
            movement.driveFor(pivotAway, turnDur, turnSpd);
            EVA_LOGLN("[EVA] Edge→PIVOT_AWAY (Rotating 90° away from drop-off)");
            finishReaction(BEHAVIOUR_EDGE_AVOID, now, turnDur + MOVE_STOP_SETTLE_MS);
        }
    }

    void reactToTap(unsigned long now) {
        driveSocial  = max(0.0f, driveSocial  - 0.30f);
        driveEnergy  = min(1.0f, driveEnergy  + 0.06f);
        driveComfort = min(1.0f, driveComfort + 0.10f);
        // Primary touch emotion: HAPPY — warm, simple, direct
        emotion.transitionTo(EVA_HAPPY, randomReactionDwell());
        syncMood(EVA_HAPPY);
        light.showEmotion(EVA_HAPPY);
        buzzer.playChirpUp();
        if (canMove()) movement.play(MoveState::HOP_FORWARD, emotion.getArousal());
        EVA_LOGLN("[EVA] Touch→TAP (HAPPY)");
        finishReaction(BEHAVIOUR_TOUCH_REACT, now, randomReactionDwell());
    }

    void reactToDoubleTap(unsigned long now) {
        driveSocial  = max(0.0f, driveSocial  - 0.40f);
        driveEnergy  = min(1.0f, driveEnergy  + 0.08f);
        driveComfort = min(1.0f, driveComfort + 0.12f);
        // Double-tap = EXCITED — more enthusiastic than a single tap
        emotion.transitionTo(EVA_EXCITED, randomReactionDwell());
        syncMood(EVA_EXCITED);
        light.showEmotion(EVA_EXCITED);
        buzzer.playForEmotion(EVA_EXCITED);
        if (canMove()) {
            MoveState move = (random(0,2)==0) ? MoveState::DONUT : MoveState::WIGGLE;
            movement.play(move, emotion.getArousal());
        }
        EVA_LOGLN("[EVA] Touch→DOUBLE_TAP (EXCITED)");
        finishReaction(BEHAVIOUR_TOUCH_REACT, now, randomReactionDwell());
    }

    void reactToPetting(unsigned long now) {
        sustainedPetCount++;
        if (sustainedPetCount > 3) {
            sustainedPetCount = 0;
            driveComfort = max(0.0f, driveComfort - 0.15f);
            // Too much petting → ANNOYED (OG primary)
            emotion.transitionTo(EVA_ANNOYED, randomReactionDwell());
            syncMood(EVA_ANNOYED);
            light.showEmotion(EVA_ANNOYED);
            buzzer.playAnnoyed();
            if (canMove()) movement.play(MoveState::WIGGLE, 0.3f);
            EVA_LOGLN("[EVA] Touch→PETTING_TOO_ROUGH (ANNOYED)");
        } else {
            driveSocial  = max(0.0f, driveSocial  - 0.20f);
            driveEnergy  = min(1.0f, driveEnergy  + 0.04f);
            driveComfort = min(1.0f, driveComfort + 0.08f);
            // Gentle petting → AFFECTIONATE (OG primary, distinct from TAP→HAPPY)
            emotion.transitionTo(EVA_AFFECTIONATE, randomReactionDwell());
            syncMood(EVA_HAPPY);
            light.showEmotion(EVA_AFFECTIONATE);
            buzzer.playPurr(false);
            if (canMove()) movement.play(MoveState::WIGGLE, 0.15f);
            EVA_LOGLN("[EVA] Touch→PETTING (AFFECTIONATE)");
        }
        finishReaction(BEHAVIOUR_TOUCH_REACT, now, randomReactionDwell());
    }

    void reactToLongHold(unsigned long now) {
        driveEnergy = max(0.0f, driveEnergy - 0.15f);
        emotion.onLongTouchHold();
        syncMood(EVA_SLEEPY);
        light.showEmotion(EVA_SLEEPY);
        buzzer.playYawn();
        sleepyRequested = true;
        finishReaction(BEHAVIOUR_SLEEPY, now, 500);
    }
};

