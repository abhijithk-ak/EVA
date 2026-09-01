#pragma once
/*
 * CommsHub.h
 * ---------------------------------------------------------
 * ONE always-on classic Bluetooth Serial connection
 * ("EVA") is the single channel for every remote capability:
 * mode switching, alarm/time management, AND RC driving.
 * Started once in setup() and alive regardless of which Mode
 * is active — this replaces the earlier idea of a separate
 * BLE service; one Bluetooth transport, one command surface.
 *
 * Text protocol (newline-terminated, works in ANY mode):
 *   MODE EVA | MODE PET | MODE CLOCK | MODE SLEEP | MODE RC
 *   TIME HH:MM
 *   ALARM HH:MM ON
 *   ALARM OFF
 *
 * Single-character protocol (RC-app compatible; only applied
 * to the motors while Mode 5 / RC is active — ignored
 * otherwise so a stray character never moves EVA by accident):
 *   F B L R G I H J S   0-9 (speed preset)
 *
 * Distinguishing the two: a lone character with nothing else
 * already queued behind it is treated as an immediate RC
 * command (matches how RC apps send one raw byte per button
 * press/hold, no newline). Anything arriving as part of a
 * longer burst is instead buffered as a text command line
 * until '\n'/'\r'. This is a practical heuristic, not a
 * hard guarantee — see README for the note on this.
 * ---------------------------------------------------------
 */

#include <Arduino.h>
#include <BluetoothSerial.h>
#include "Types.h"
#include "Config.h"
#include "ModeEngine.h"
#include "ClockService.h"
#include "MovementEngine.h"

class CommsHub {
public:
    CommsHub(ModeEngine &modeEngineRef, ClockService &clockRef, MovementEngine &movementRef);

    void begin();
    void update(); // call every loop()

    bool isConnected();

private:
    BluetoothSerial bt;
    ModeEngine &modeEngine;
    ClockService &clock;
    MovementEngine &movement;

    String lineBuffer;
    uint8_t rcSpeed;
    unsigned long lastRcCommandTime;

    void handleByte(char c);
    void executeTextCommand(String cmd);
    void executeMovementChar(char c);
    void rcSafetyWatchdog();
};
