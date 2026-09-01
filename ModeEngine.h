#pragma once
/*
 * ModeEngine.h
 * ---------------------------------------------------------
 * Owns the currently active Mode and enforces the global
 * transition rules that are not local to any single mode:
 *
 *   - long touch hold (any mode that reports it) -> Sleep
 *   - tap while asleep                            -> EVA
 *   - explicit mode switch request (serial command
 *     today; a physical button / BLE command in a future
 *     revision) -> requested mode
 *
 * Individual Mode objects still decide their OWN internal
 * behaviour; ModeEngine only decides WHICH mode is active.
 * ---------------------------------------------------------
 */

#include <Arduino.h>
#include "Mode.h"
#include "Types.h"
#include "Logger.h"

class ModeEngine {
public:
    ModeEngine() : current(nullptr), currentId(MODE_EVA) {
        for (int i = 0; i < 5; i++) modes[i] = nullptr;
    }

    void registerMode(Mode *mode) {
        modes[static_cast<int>(mode->id())] = mode;
    }

    void begin(EvaMode startMode) {
        switchTo(startMode);
    }

    void switchTo(EvaMode id) {
        if (current) current->exit();
        current = modes[static_cast<int>(id)];
        currentId = id;
        if (current) {
            EVA_LOGF("[ModeEngine] switching to mode %d\n", (int)id);
            current->enter();
        }
    }

    void update() {
        if (!current) return;
        current->update();
        handleSerialOverride();
    }

    Mode *getCurrent() const { return current; }
    EvaMode getCurrentId() const { return currentId; }

private:
    Mode *modes[5];
    Mode *current;
    EvaMode currentId;

    // Simple text-command mode switch over USB serial for bring-up,
    // testing, and manual demoing. A physical button or Bluetooth
    // command can call switchTo() directly in a future revision.
    void handleSerialOverride() {
#if EVA_DEBUG
        if (!Serial.available()) return;
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        cmd.toUpperCase();

        if (cmd == "MODE EVA")   switchTo(MODE_EVA);
        else if (cmd == "MODE PET")   switchTo(MODE_PET);
        else if (cmd == "MODE CLOCK") switchTo(MODE_CLOCK);
        else if (cmd == "MODE SLEEP") switchTo(MODE_SLEEP);
        else if (cmd == "MODE RC")    switchTo(MODE_RC);
#endif
    }
};
