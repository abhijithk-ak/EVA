#pragma once
/*
 * ModeClock.h
 * ---------------------------------------------------------
 * Mode 3 - Clock Mode. Movement is fully disabled. RoboEyes
 * rendering is paused while this mode owns the OLED directly.
 *
 * Layout follows the same visual hierarchy as the reference
 * design (top status row, date/day row, giant centered time,
 * bottom info panel) - see PROJECT_DOCUMENTATION.md for the
 * note on why full pixel-icon fidelity isn't attempted on a
 * 128x64 monochrome panel.
 *
 * Alarm setup (touch-driven, non-blocking):
 *   PETTING (5-10s hold)  -> cycle Normal -> Set Hour -> Set Minute -> Normal
 *   TAP while Set Hour    -> hour + 1 (wraps 0-23)
 *   TAP while Set Minute  -> minute + 5 (wraps 0-59)
 *   TAP while Normal      -> toggle alarm enabled/disabled
 *   8s with no taps while in a Set state -> auto-commit and return
 * (Each tap now advances the field by exactly one step - the old
 * "hour jumps by 2 / minute jumps by 10" bug was a TouchManager
 * event-replay issue, fixed at the source - see TouchManager.h.)
 * ---------------------------------------------------------
 */

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Mode.h"
#include "Types.h"
#include "Config.h"
#include "ClockService.h"
#include "BuzzerManager.h"
#include "TouchManager.h"
#include "MovementEngine.h"

class ModeClock : public Mode {
public:
    ModeClock(Adafruit_SSD1306 &displayRef,
              ClockService &clockRef,
              BuzzerManager &buzzerRef,
              TouchManager &touchRef,
              MovementEngine &movementRef);

    void enter() override;
    void update() override;
    void exit() override;
    EvaMode id() const override { return MODE_CLOCK; }
    bool ownsDisplay() const override { return true; }

private:
    enum class UiState { NORMAL, SET_HOUR, SET_MINUTE };

    Adafruit_SSD1306 &display;
    ClockService &clock;
    BuzzerManager &buzzer;
    TouchManager &touch;
    MovementEngine &movement;

    UiState uiState;
    unsigned long lastSetupInteraction;
    unsigned long lastInfoSwap;
    bool showingWeather;

    AlarmSetting pendingAlarm;

    bool alarmRinging;
    bool alarmSnoozed;
    uint8_t alarmRingCycle;
    unsigned long alarmRingStartMs;
    unsigned long alarmLastToneMs;
    unsigned long alarmNextRingMs;
    unsigned long alarmTapWindowStartMs;
    uint8_t alarmTapCount;
    unsigned long lastTapProcessedMs;

    void handleTouch();
    void handleAlarmState();
    void beginAlarmRing();
    void snoozeAlarm();
    void stopAlarmRing();
    void render();
    void drawNormal();
    void drawSetup();
};
