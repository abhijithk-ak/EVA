#pragma once
/*
 * ModeClock.h
 * ---------------------------------------------------------
 * Mode 3 - Clock Mode. Movement is fully disabled. RoboEyes
 * rendering is paused while this mode owns the OLED directly.
 *
 * UI layout (128x64 monochrome OLED, matches reference image):
 *
 *   Row 0-8:  [Bell] ALM ON/OFF               [WiFi/X] [BT/X]
 *   Row 9-17: [Cal ] DD/MM/YYYY                    SAT
 *             (no divider line between header and time)
 *   Row 18-44: Large centered HH:MM            AM/PM
 *   Row 45:   ────────────────────────────── (divider)
 *   Row 46-55: Location        [icon] Condition
 *   Row 56-63: Temp°C
 *
 * WiFi and BT icons are replaced by an X bitmap when offline/disconnected.
 * Day name is always 3-letter abbreviation (SAT, FRI, etc.).
 *
 * Alarm setup (touch-driven, non-blocking):
 *   PETTING (5-10s hold)  -> cycle Normal -> Set Hour -> Set Minute -> Normal
 *   TAP while Set Hour    -> hour + 1 (wraps 0-23)
 *   TAP while Set Minute  -> minute + 5 (wraps 0-59)
 *   TAP while Normal      -> toggle alarm enabled/disabled
 *   8s with no taps while in a Set state -> auto-commit and return
 * ---------------------------------------------------------
 */

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Mode.h"
#include "Types.h"
#include "Config.h"
#include "ClockService.h"
#include "ClockIcons.h"
#include "BuzzerManager.h"
#include "TouchManager.h"
#include "MovementEngine.h"

// Forward declaration — full CommsHub.h included only in ModeClock.cpp
// to check BT connection status without creating circular includes.
class CommsHub;

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

    // Wire up CommsHub after construction so drawNormal() can read BT status.
    // Call once in setup() from EVA.ino. Safe to skip (BT icon shows X).
    void setCommsHub(CommsHub *p) { pComms = p; }

private:
    enum class UiState { NORMAL, SET_HOUR, SET_MINUTE };

    Adafruit_SSD1306 &display;
    ClockService     &clock;
    BuzzerManager    &buzzer;
    TouchManager     &touch;
    MovementEngine   &movement;
    CommsHub         *pComms = nullptr; // set via setCommsHub(); null = BT unknown

    UiState       uiState;
    unsigned long lastSetupInteraction;

    AlarmSetting pendingAlarm;

    bool          alarmRinging;
    bool          alarmSnoozed;
    uint8_t       alarmRingCycle;
    unsigned long alarmRingStartMs;
    unsigned long alarmLastToneMs;
    unsigned long alarmNextRingMs;
    unsigned long alarmTapWindowStartMs;
    uint8_t       alarmTapCount;
    unsigned long lastTapProcessedMs;

    void handleTouch();
    void handleAlarmState();
    void beginAlarmRing();
    void snoozeAlarm();
    void stopAlarmRing();
    void render();
    void drawNormal();
    void drawSetup();

    // Returns the PROGMEM icon bitmap to use for a given weather condition string.
    const unsigned char* weatherConditionIcon(const String &condition);
};
