#include "MovementEngine.h"
#include "Config.h"
#include "Logger.h"

MovementEngine::MovementEngine()
    : currentState(MoveState::STOP),
      currentSpeed(0),
      timedMoveActive(false),
      moveStartTime(0),
      moveDuration(0),
      locked(false),
      danceStepCount(0),
      currentDanceStep(0),
      dancingActive(false),
      inDanceStepPause(false),
      danceStepStartTime(0) {}

void MovementEngine::begin() {
    pinMode(PIN_MOTOR_LF, OUTPUT);
    pinMode(PIN_MOTOR_LB, OUTPUT);
    pinMode(PIN_MOTOR_RB, OUTPUT);
    pinMode(PIN_MOTOR_RF, OUTPUT);

    setupPwmChannel(PIN_MOTOR_LF, PWM_CH_LF);
    setupPwmChannel(PIN_MOTOR_LB, PWM_CH_LB);
    setupPwmChannel(PIN_MOTOR_RB, PWM_CH_RB);
    setupPwmChannel(PIN_MOTOR_RF, PWM_CH_RF);

    stop();
}

void MovementEngine::setupPwmChannel(uint8_t pin, uint8_t channel) {
    ledcSetup(channel, PWM_FREQ_HZ, PWM_RESOLUTION_BIT);
    ledcAttachPin(pin, channel);
    ledcWrite(channel, 0);
}

void MovementEngine::writeMotor(uint8_t channelA, uint8_t channelB, int16_t signedSpeed) {
    signedSpeed = constrain(signedSpeed, -255, 255);
    if (signedSpeed >= 0) {
        ledcWrite(channelA, signedSpeed);
        ledcWrite(channelB, 0);
    } else {
        ledcWrite(channelA, 0);
        ledcWrite(channelB, -signedSpeed);
    }
}

void MovementEngine::applyToMotors(MoveState state, uint8_t speed) {
    if (speed == 0) speed = MOVE_DEFAULT_SPEED;

    switch (state) {
        case MoveState::STOP:
            writeMotor(PWM_CH_LF, PWM_CH_LB, 0);
            writeMotor(PWM_CH_RF, PWM_CH_RB, 0);
            break;

        case MoveState::FORWARD:
            writeMotor(PWM_CH_LF, PWM_CH_LB, speed);
            writeMotor(PWM_CH_RF, PWM_CH_RB, speed);
            break;

        case MoveState::BACKWARD:
            writeMotor(PWM_CH_LF, PWM_CH_LB, -speed);
            writeMotor(PWM_CH_RF, PWM_CH_RB, -speed);
            break;

        case MoveState::PIVOT_LEFT:
            writeMotor(PWM_CH_LF, PWM_CH_LB, -speed);
            writeMotor(PWM_CH_RF, PWM_CH_RB,  speed);
            break;

        case MoveState::PIVOT_RIGHT:
            writeMotor(PWM_CH_LF, PWM_CH_LB,  speed);
            writeMotor(PWM_CH_RF, PWM_CH_RB, -speed);
            break;

        case MoveState::TURN_LEFT:
            writeMotor(PWM_CH_LF, PWM_CH_LB, 0);
            writeMotor(PWM_CH_RF, PWM_CH_RB, speed);
            break;

        case MoveState::TURN_RIGHT:
            writeMotor(PWM_CH_LF, PWM_CH_LB, speed);
            writeMotor(PWM_CH_RF, PWM_CH_RB, 0);
            break;

        case MoveState::CURVE_LEFT:
            writeMotor(PWM_CH_LF, PWM_CH_LB, (int16_t)(speed * 0.45f));
            writeMotor(PWM_CH_RF, PWM_CH_RB, speed);
            break;

        case MoveState::CURVE_RIGHT:
            writeMotor(PWM_CH_LF, PWM_CH_LB, speed);
            writeMotor(PWM_CH_RF, PWM_CH_RB, (int16_t)(speed * 0.45f));
            break;

        case MoveState::WIGGLE:
            // Excited puppy chassis shake
            writeMotor(PWM_CH_LF, PWM_CH_LB,  speed);
            writeMotor(PWM_CH_RF, PWM_CH_RB, -speed);
            break;

        case MoveState::DONUT:
            // Full multi-rotation 360 degree spin
            writeMotor(PWM_CH_LF, PWM_CH_LB,  (int16_t)min(240, speed + 35));
            writeMotor(PWM_CH_RF, PWM_CH_RB, -(int16_t)min(240, speed + 35));
            break;

        case MoveState::HOP_FORWARD:
            // Energetic short forward hop
            writeMotor(PWM_CH_LF, PWM_CH_LB, (int16_t)min(255, speed + 35));
            writeMotor(PWM_CH_RF, PWM_CH_RB, (int16_t)min(255, speed + 35));
            break;

        case MoveState::FLINCH_BACK:
            // Sharp fast backward flinch
            writeMotor(PWM_CH_LF, PWM_CH_LB, -(int16_t)min(255, speed + 50));
            writeMotor(PWM_CH_RF, PWM_CH_RB, -(int16_t)min(255, speed + 50));
            break;

        case MoveState::JERK_STEPS:
            writeMotor(PWM_CH_LF, PWM_CH_LB, 200);
            writeMotor(PWM_CH_RF, PWM_CH_RB, 200);
            break;

        case MoveState::SLOW_FORWARD:
            writeMotor(PWM_CH_LF, PWM_CH_LB, 110);
            writeMotor(PWM_CH_RF, PWM_CH_RB, 110);
            break;

        case MoveState::SLOW_CURVE_FWD:
            writeMotor(PWM_CH_LF, PWM_CH_LB, 140);
            writeMotor(PWM_CH_RF, PWM_CH_RB, 65);
            break;

        case MoveState::SLOW_CURVE_REV:
            writeMotor(PWM_CH_LF, PWM_CH_LB, -140);
            writeMotor(PWM_CH_RF, PWM_CH_RB, -65);
            break;
    }
}

void MovementEngine::drive(MoveState state, uint8_t speed) {
    if (locked) return;
    dancingActive = false;
    timedMoveActive = false;
    currentState = state;
    currentSpeed = speed;
    applyToMotors(state, speed);
}

void MovementEngine::stop() {
    dancingActive = false;
    timedMoveActive = false;
    currentState = MoveState::STOP;
    currentSpeed = 0;
    applyToMotors(MoveState::STOP, 0);
    ledcWrite(PWM_CH_LF, 0);
    ledcWrite(PWM_CH_LB, 0);
    ledcWrite(PWM_CH_RF, 0);
    ledcWrite(PWM_CH_RB, 0);
}

void MovementEngine::driveFor(MoveState state, unsigned long durationMs, uint8_t speed) {
    if (locked) return;
    dancingActive = false;
    currentState = state;
    currentSpeed = speed;
    applyToMotors(state, speed);

    timedMoveActive = true;
    moveStartTime = millis();
    moveDuration = durationMs;
}

void MovementEngine::play(MoveState type, float arousal) {
    if (locked) return;
    arousal = constrain(arousal, 0.0f, 1.0f);
    uint8_t spd = map((int)(arousal * 100.0f), 0, 100, 95, 220);

    unsigned long duration = 600;
    switch (type) {
        case MoveState::FORWARD:        duration = map((int)(arousal * 100.0f), 0, 100, 400, 800); break;
        case MoveState::BACKWARD:       duration = map((int)(arousal * 100.0f), 0, 100, 350, 650); break;
        case MoveState::TURN_LEFT:
        case MoveState::PIVOT_LEFT:
        case MoveState::TURN_RIGHT:
        case MoveState::PIVOT_RIGHT:    duration = 350; break;
        case MoveState::CURVE_LEFT:
        case MoveState::CURVE_RIGHT:    duration = 1100; break;
        case MoveState::DONUT:          duration = 1500; spd = 210; break;
        case MoveState::WIGGLE:         duration = 850; break;
        case MoveState::HOP_FORWARD:    duration = 220; break;
        case MoveState::FLINCH_BACK:    duration = 200; spd = min((uint8_t)240, (uint8_t)(spd + 30)); break;
        case MoveState::JERK_STEPS:     duration = 350; break;
        case MoveState::SLOW_FORWARD:   duration = 800; break;
        case MoveState::SLOW_CURVE_FWD: duration = 950; break;
        case MoveState::SLOW_CURVE_REV: duration = 950; break;
        default: break;
    }

    driveFor(type, duration, spd);
}

// =========================================================================
// Procedural Dance Sequencer
// Choreography specified by user:
// 1. Move Forward (Flourish/Hop)
// 2. Forward Pivot L & R
// 3. Backward Pivot L & R
// 4. Double Pivot Left (forward/backward preference)
// 5. Double Pivot Right (opposite preference)
// 6. Grand Finale: 360 Spin OR smooth slow backward glide!
// Mix-and-match randomization ensures unique dance routines every time.
// =========================================================================
void MovementEngine::startDanceRoutine() {
    if (locked) return;

    timedMoveActive = false;
    danceStepCount = 0;

    // Preference selector: 0 = forward-biased double pivot, 1 = backward-biased
    bool leftPreferForward = (random(0, 2) == 0);

    // Step 1: Forward flourish
    danceQueue[danceStepCount++] = { MoveState::HOP_FORWARD, (unsigned long)random(250, 380), (uint8_t)random(160, 190), 80 };

    // Step 2 & 3: Forward Pivot Left & Right
    if (random(0, 2) == 0) {
        danceQueue[danceStepCount++] = { MoveState::PIVOT_LEFT,  (unsigned long)random(260, 350), (uint8_t)random(170, 200), 70 };
        danceQueue[danceStepCount++] = { MoveState::PIVOT_RIGHT, (unsigned long)random(260, 350), (uint8_t)random(170, 200), 80 };
    } else {
        danceQueue[danceStepCount++] = { MoveState::PIVOT_RIGHT, (unsigned long)random(260, 350), (uint8_t)random(170, 200), 70 };
        danceQueue[danceStepCount++] = { MoveState::PIVOT_LEFT,  (unsigned long)random(260, 350), (uint8_t)random(170, 200), 80 };
    }

    // Step 4: Backward step & pivots
    danceQueue[danceStepCount++] = { MoveState::BACKWARD,    (unsigned long)random(280, 420), (uint8_t)random(140, 175), 90 };
    danceQueue[danceStepCount++] = { MoveState::PIVOT_LEFT,  (unsigned long)random(240, 330), (uint8_t)random(160, 190), 60 };
    danceQueue[danceStepCount++] = { MoveState::PIVOT_RIGHT, (unsigned long)random(240, 330), (uint8_t)random(160, 190), 80 };

    // Step 5: Double Pivot Left (with selected forward/backward bias)
    MoveState dblLeft = leftPreferForward ? MoveState::CURVE_LEFT : MoveState::PIVOT_LEFT;
    danceQueue[danceStepCount++] = { dblLeft, (unsigned long)random(220, 320), (uint8_t)random(170, 200), 60 };
    danceQueue[danceStepCount++] = { dblLeft, (unsigned long)random(220, 320), (uint8_t)random(170, 200), 90 };

    // Step 6: Double Pivot Right (with opposite bias)
    MoveState dblRight = (!leftPreferForward) ? MoveState::CURVE_RIGHT : MoveState::PIVOT_RIGHT;
    danceQueue[danceStepCount++] = { dblRight, (unsigned long)random(220, 320), (uint8_t)random(170, 200), 60 };
    danceQueue[danceStepCount++] = { dblRight, (unsigned long)random(220, 320), (uint8_t)random(170, 200), 100 };

    // Step 7: Grand Finale! (Mix-and-match finale)
    int finaleChoice = random(0, 100);
    if (finaleChoice < 55) {
        // 360 Donut Spin
        danceQueue[danceStepCount++] = { MoveState::DONUT, (unsigned long)random(1200, 1600), 220, 0 };
    } else {
        // Smooth slow backward glide + Joyful chassis wiggle
        danceQueue[danceStepCount++] = { MoveState::SLOW_CURVE_REV, (unsigned long)random(600, 900), 130, 80 };
        danceQueue[danceStepCount++] = { MoveState::WIGGLE,         (unsigned long)random(500, 800), 170, 0 };
    }

    currentDanceStep = 0;
    dancingActive = true;
    inDanceStepPause = false;
    danceStepStartTime = millis();

    // Start first step
    const DanceStep &step = danceQueue[0];
    currentState = step.move;
    currentSpeed = step.speed;
    applyToMotors(currentState, currentSpeed);

    EVA_LOGF("[MovementEngine] Started Procedural Dance Routine (%u steps)\n", danceStepCount);
}

bool MovementEngine::isDancing() const {
    return dancingActive;
}

void MovementEngine::stopDance() {
    if (dancingActive) {
        dancingActive = false;
        stop();
    }
}

void MovementEngine::advanceDanceStep() {
    currentDanceStep++;
    if (currentDanceStep >= danceStepCount) {
        // Dance routine finished!
        dancingActive = false;
        stop();
        EVA_LOGLN("[MovementEngine] Dance routine completed!");
        return;
    }

    inDanceStepPause = false;
    danceStepStartTime = millis();
    const DanceStep &step = danceQueue[currentDanceStep];
    currentState = step.move;
    currentSpeed = step.speed;
    applyToMotors(currentState, currentSpeed);
}

void MovementEngine::lock() {
    locked = true;
    stop();
}

void MovementEngine::unlock() {
    locked = false;
}

bool MovementEngine::isLocked() const {
    return locked;
}

void MovementEngine::update() {
    if (locked) {
        if (currentState != MoveState::STOP) {
            stop();
        }
        return;
    }

    unsigned long now = millis();

    // Procedural Dance Step Execution
    if (dancingActive) {
        const DanceStep &step = danceQueue[currentDanceStep];
        unsigned long elapsed = now - danceStepStartTime;

        if (!inDanceStepPause) {
            if (elapsed >= step.durationMs) {
                if (step.pauseAfterMs > 0) {
                    inDanceStepPause = true;
                    danceStepStartTime = now;
                    applyToMotors(MoveState::STOP, 0);
                } else {
                    advanceDanceStep();
                }
            } else if (currentState == MoveState::WIGGLE) {
                if ((elapsed / 140) % 2 == 0) {
                    writeMotor(PWM_CH_LF, PWM_CH_LB, -(int16_t)(currentSpeed * 0.5f));
                    writeMotor(PWM_CH_RF, PWM_CH_RB,  (int16_t)(currentSpeed * 0.5f));
                } else {
                    writeMotor(PWM_CH_LF, PWM_CH_LB,  (int16_t)(currentSpeed * 0.5f));
                    writeMotor(PWM_CH_RF, PWM_CH_RB, -(int16_t)(currentSpeed * 0.5f));
                }
            }
        } else {
            // In pause between steps
            if (elapsed >= step.pauseAfterMs) {
                advanceDanceStep();
            }
        }
        return;
    }

    // Standard timed movement execution
    if (timedMoveActive) {
        unsigned long elapsed = now - moveStartTime;
        if (elapsed >= moveDuration) {
            stop();
        } else if (currentState == MoveState::WIGGLE) {
            if ((elapsed / 150) % 2 == 0) {
                writeMotor(PWM_CH_LF, PWM_CH_LB, -(int16_t)(currentSpeed * 0.5f));
                writeMotor(PWM_CH_RF, PWM_CH_RB,  (int16_t)(currentSpeed * 0.5f));
            } else {
                writeMotor(PWM_CH_LF, PWM_CH_LB,  (int16_t)(currentSpeed * 0.5f));
                writeMotor(PWM_CH_RF, PWM_CH_RB, -(int16_t)(currentSpeed * 0.5f));
            }
        }
    }
}

bool MovementEngine::isMoving() const {
    return currentState != MoveState::STOP;
}

MoveState MovementEngine::getState() const {
    return currentState;
}
