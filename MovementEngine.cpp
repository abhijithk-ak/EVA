#include "MovementEngine.h"
#include "Config.h"
#include "Logger.h"

MovementEngine::MovementEngine()
    : currentState(MoveState::STOP),
      currentSpeed(0),
      timedMoveActive(false),
      moveStartTime(0),
      moveDuration(0),
      locked(false) {}

void MovementEngine::begin() {
    pinMode(PIN_MOTOR_LF, OUTPUT);
    pinMode(PIN_MOTOR_LB, OUTPUT);
    pinMode(PIN_MOTOR_RB, OUTPUT);
    pinMode(PIN_MOTOR_RF, OUTPUT);

    // NOTE: Uses the classic Arduino-ESP32 core v2.x LEDC API
    // (ledcSetup/ledcAttachPin/ledcWrite). If you are on
    // Arduino-ESP32 core v3.x, swap these three calls for the
    // newer ledcAttach(pin, freq, resolution) equivalent —
    // see the project README for both variants.
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
            writeMotor(PWM_CH_LF, PWM_CH_LB, speed * 0.5f);
            writeMotor(PWM_CH_RF, PWM_CH_RB, speed);
            break;

        case MoveState::CURVE_RIGHT:
            writeMotor(PWM_CH_LF, PWM_CH_LB, speed);
            writeMotor(PWM_CH_RF, PWM_CH_RB, speed * 0.5f);
            break;
    }
}

void MovementEngine::drive(MoveState state, uint8_t speed) {
    if (locked) return; // e.g. ModeClock owns the robot right now - ignore
    timedMoveActive = false; // manual drive cancels any timed move
    currentState = state;
    currentSpeed = speed;
    applyToMotors(state, speed);
}

void MovementEngine::stop() {
    // stop() is intentionally NOT gated by `locked` - callers must always
    // be able to force the motors off, lock or no lock.
    timedMoveActive = false;
    currentState = MoveState::STOP;
    currentSpeed = 0;
    applyToMotors(MoveState::STOP, 0);
}

void MovementEngine::driveFor(MoveState state, unsigned long durationMs, uint8_t speed) {
    if (locked) return; // ignore timed moves while locked too
    currentState = state;
    currentSpeed = speed;
    applyToMotors(state, speed);

    timedMoveActive = true;
    moveStartTime = millis();
    moveDuration = durationMs;
}

void MovementEngine::lock() {
    locked = true;
    stop(); // make sure nothing is mid-motion the instant we lock
}

void MovementEngine::unlock() {
    locked = false;
}

bool MovementEngine::isLocked() const {
    return locked;
}

void MovementEngine::update() {
    if (timedMoveActive && (millis() - moveStartTime >= moveDuration)) {
        stop();
    }
}

bool MovementEngine::isMoving() const {
    return currentState != MoveState::STOP;
}

MoveState MovementEngine::getState() const {
    return currentState;
}
