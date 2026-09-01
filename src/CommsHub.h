#pragma once
/*
 * CommsHub.h
 * ---------------------------------------------------------
 * ONE always-on classic Bluetooth Serial connection ("EVA")
 * handles mode switching, alarm/time management, RC driving,
 * AND special trick/emotion triggers (DANCE, TRICK, SCARE,
 * RANDOM, EMOTION <NAME>).
 * ---------------------------------------------------------
 */

#include <Arduino.h>
#include <BluetoothSerial.h>
#include <Adafruit_SSD1306.h>
#include "Types.h"
#include "Config.h"
#include "ModeEngine.h"
#include "ClockService.h"
#include "BuzzerManager.h"
#include "MovementEngine.h"

// Forward declaration of templated engine runner
class IEmotionRunner {
public:
    virtual void triggerEmotion(EvaEmotion emo) = 0;
    virtual void triggerDance() = 0;
    virtual void triggerSpin() = 0;
    virtual void triggerTrick() = 0;
    virtual void triggerScare() = 0;
    virtual void triggerRandom() = 0;
    virtual void triggerAnimation(uint8_t animId) = 0;
    virtual void setAnimationsEnabled(bool en) = 0;
    virtual void startDinoGame() = 0;
    virtual void jumpDino() = 0;
};

class CommsHub {
public:
    CommsHub(ModeEngine &modeEngineRef, ClockService &clockRef,
             BuzzerManager &buzzerRef, MovementEngine &movementRef,
             Adafruit_SSD1306 &displayRef);

    void begin();
    void update();

    bool isConnected();
    void setBrightness(uint8_t level);
    void setEmotionRunner(IEmotionRunner *runner);

private:
    BluetoothSerial   bt;
    ModeEngine       &modeEngine;
    ClockService     &clock;
    BuzzerManager    &buzzer;
    MovementEngine   &movement;
    Adafruit_SSD1306 &display;
    IEmotionRunner   *emotionRunner;

    String lineBuffer;
    uint8_t rcSpeed;
    unsigned long lastRcCommandTime;

    void handleByte(char c);
    void executeTextCommand(String cmd);
    void executeMovementChar(char c);
    void rcSafetyWatchdog();
};
