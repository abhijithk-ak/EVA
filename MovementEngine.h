#pragma once
/*
 * MovementEngine.h
 * ---------------------------------------------------------
 * Non-blocking replacement for the Stage 0.1 delay()-based
 * motor prototype. Drives 2x DC N20 motors through a DRV8833
 * via 4 PWM-capable direction pins.
 *
 * Public vocabulary is behavioural ("move forward for a bit",
 * "turn away"), not raw pin writes — callers (BehaviourEngine,
 * ModeRC) never touch GPIO directly.
 *
 * No encoders are fitted, so this is timed, open-loop
 * movement, not precision navigation.
 * ---------------------------------------------------------
 */

#include <Arduino.h>
#include "Types.h"

class MovementEngine {
public:
    MovementEngine();

    void begin();

    // Call every loop() — advances any timed movement and
    // enforces safe-stop when a movement's duration elapses.
    void update();

    // ---- Immediate (untimed) commands, used by RC mode ----
    void drive(MoveState state, uint8_t speed = 0);
    void stop();

    // ---- Behaviourally-timed commands, used by BehaviourEngine ----
    // Moves for durationMs then automatically returns to STOP.
    void driveFor(MoveState state, unsigned long durationMs, uint8_t speed = 0);

    bool isMoving() const;
    MoveState getState() const;

    // Hard motor-safety gate. While locked, drive()/driveFor() are
    // no-ops for ANY caller (BehaviourEngine, ModeRC, CommsHub's
    // always-on Bluetooth RC channel, etc.) — stop() still always
    // works. ModeClock locks this on enter() and unlocks on exit()
    // so nothing can spin the motors while the clock/alarm UI owns
    // the robot.
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

    void applyToMotors(MoveState state, uint8_t speed);
    void setupPwmChannel(uint8_t pin, uint8_t channel);
    void writeMotor(uint8_t channelA, uint8_t channelB, int16_t signedSpeed);
    // signedSpeed > 0 drives channelA (forward-like), < 0 drives channelB
};
