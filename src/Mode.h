#pragma once
/*
 * Mode.h
 * ---------------------------------------------------------
 * Common interface for the 5 top-level operating modes.
 * ModeEngine owns one instance of each and calls enter()
 * once on transition, update() every loop() while active,
 * and exit() once when leaving.
 * ---------------------------------------------------------
 */

#include "Types.h"

class Mode {
public:
    virtual ~Mode() {}
    virtual void enter() = 0;
    virtual void update() = 0;
    virtual void exit() = 0;
    virtual EvaMode id() const = 0;

    // True while this mode is drawing directly to the OLED itself
    // (e.g. Clock UI, Sleep's periodic time display). When true,
    // the main loop skips roboEyes.update() so the two renderers
    // never fight over the same framebuffer.
    virtual bool ownsDisplay() const { return false; }
};
