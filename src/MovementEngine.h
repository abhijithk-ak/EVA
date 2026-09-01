#pragma once
/*
 * MovementEngine.h
 * ---------------------------------------------------------
 * Non-blocking motor driver & procedural choreography engine.
 * Drives 2x DC N20 motors through DRV8833 via 4 PWM channels.
 *
 * Provides:
 * - Behavioural movement primitives (driveFor, play)
 * - Immediate RC driving (drive, stop)
 * - Procedural multi-phase Dance Routine Sequencer (non-blocking)
 * - Safety interlocks & hardware lock gates
 * ---------------------------------------------------------
 */

#include <Arduino.h>
#include "Types.h"

struct DanceStep {
    MoveState move;
    unsigned long durationMs;
    uint8_t speed;
    unsigned long pauseAfterMs;
};

class MovementEngine {
public:
    MovementEngine();

    void begin();

    // Call every loop() — advances timed movements and dance steps
    void update();

    // ---- Immediate (untimed) commands, used by RC mode ----
    void drive(MoveState state, uint8_t speed = 0);
    void stop();

    // ---- Behaviourally-timed commands ----
    void driveFor(MoveState state, unsigned long durationMs, uint8_t speed = 0);

    // EMO Gesture primitive playback scaled by arousal (0.0 - 1.0)
    void play(MoveState type, float arousal = 0.5f);

    // ---- Procedural Dance Sequencer ----
    void startDanceRoutine();
    bool isDancing() const;
    void stopDance();

    bool isMoving() const;
    MoveState getState() const;

    // Hard motor-safety gate
    void lock();
    void unlock();
    bool isLocked() const;

private:
    MoveState currentState;
    uint8_t currentSpeed;

    bool timedMoveActive;
    unsigned long moveStartTime;
    unsigned long moveDuration;

    bool locked;

    // Dance state machine
    static const uint8_t MAX_DANCE_STEPS = 14;
    DanceStep danceQueue[MAX_DANCE_STEPS];
    uint8_t danceStepCount;
    uint8_t currentDanceStep;
    bool dancingActive;
    bool inDanceStepPause;
    unsigned long danceStepStartTime;

    void advanceDanceStep();
    void applyToMotors(MoveState state, uint8_t speed);
    void setupPwmChannel(uint8_t pin, uint8_t channel);
    void writeMotor(uint8_t channelA, uint8_t channelB, int16_t signedSpeed);
};
