/*
 * EVA.ino
 * ===========================================================
 * EVA — Behaviour-Based Desktop Companion Robot
 * Main sketch: hardware bring-up + wiring of all engines,
 * managers and modes. All actual logic lives in the other
 * files in this sketch folder — this file should stay small.
 *
 *   Sensor -> Interpretation -> Behaviour -> Emotion -> Expression/Movement
 *
 * See PROJECT_DOCUMENTATION.md for full architecture details.
 * ===========================================================
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <FluxGarage_RoboEyes.h>

#include "Config.h"
#include "Types.h"
#include "Logger.h"

#include "EmotionEngine.h"
#include "BehaviourEngine.h"
#include "MovementEngine.h"
#include "SensorManager.h"
#include "TouchManager.h"
#include "SoundManager.h"
#include "MoodLightManager.h"
#include "BuzzerManager.h"
#include "ClockService.h"

#include "Mode.h"
#include "ModeEngine.h"
#include "ModeEva.h"
#include "ModePet.h"
#include "ModeClock.h"
#include "ModeSleep.h"
#include "ModeRC.h"
#include "CommsHub.h"

// ---------------------------------------------------------
// Display + expression renderer
// ---------------------------------------------------------
Adafruit_SSD1306 display(EVA_SCREEN_WIDTH, EVA_SCREEN_HEIGHT, &Wire, EVA_OLED_RESET);
RoboEyes<Adafruit_SSD1306> roboEyes(display);

// ---------------------------------------------------------
// Expression + behaviour engines
// ---------------------------------------------------------
EmotionEngine<decltype(roboEyes)> emotion(roboEyes);
MovementEngine movement;
MoodLightManager light;
BuzzerManager buzzer;
BehaviourEngine<decltype(emotion)> behaviour(emotion, movement, light, buzzer);

// ---------------------------------------------------------
// Sensor interpretation layer
// ---------------------------------------------------------
SensorManager spatialSensor;
TouchManager touchSensor;
SoundManager soundSensor;

// ---------------------------------------------------------
// Clock Mode services
// ---------------------------------------------------------
ClockService clockService;

// ---------------------------------------------------------
// Cross-mode transition flags (set by a Mode, consumed by
// ModeEngine in loop() — this is the one piece of shared
// mutable state modes are allowed to signal through).
// ---------------------------------------------------------
bool sleepRequested = false;
bool wakeRequested = false;

// ---------------------------------------------------------
// Modes
// ---------------------------------------------------------
ModeEva<decltype(behaviour)>   modeEva(behaviour, spatialSensor, touchSensor, soundSensor, sleepRequested);
ModePet<decltype(behaviour)>   modePet(behaviour, touchSensor, soundSensor, sleepRequested);
ModeClock                       modeClock(display, clockService, buzzer, touchSensor, movement);
ModeSleep<decltype(emotion), decltype(roboEyes)> modeSleep(emotion, roboEyes, display, clockService, touchSensor, wakeRequested);
ModeRC<decltype(emotion)>       modeRC(emotion, movement);

ModeEngine modeEngine;

// ---------------------------------------------------------
// Remote control: one always-on Bluetooth Serial channel for
// mode switching, alarm/time commands, AND RC driving. See
// CommsHub.h for the protocol.
// ---------------------------------------------------------
CommsHub commsHub(modeEngine, clockService, movement);

void setup() {
    EVA_LOG_BEGIN();
    EVA_LOGLN("\n[EVA] booting...");

    Wire.begin(EVA_I2C_SDA, EVA_I2C_SCL);

    if (!display.begin(SSD1306_SWITCHCAPVCC, EVA_OLED_I2C_ADDR)) {
        EVA_LOGLN("[EVA] SSD1306 init FAILED - halting");
        while (true) { delay(1000); } // nothing useful can run without the display
    }

    roboEyes.begin(EVA_SCREEN_WIDTH, EVA_SCREEN_HEIGHT, 100);
    roboEyes.setAutoblinker(ON, 3, 2);
    roboEyes.setIdleMode(ON, 2, 2);

    movement.begin();
    light.begin();
    buzzer.begin();
    touchSensor.begin();
    soundSensor.begin();
    if (!spatialSensor.begin()) {
        EVA_LOGLN("[EVA] continuing without spatial awareness");
    }

    emotion.setNeutral();
    light.showEmotion(EVA_NEUTRAL);

    randomSeed(esp_random());

    modeEngine.registerMode(&modeEva);
    modeEngine.registerMode(&modePet);
    modeEngine.registerMode(&modeClock);
    modeEngine.registerMode(&modeSleep);
    modeEngine.registerMode(&modeRC);
    modeEngine.begin(MODE_EVA);

    commsHub.begin(); // Bluetooth "EVA" - always on, independent of mode

    EVA_LOGLN("[EVA] ready");
}

void loop() {
    // Cross-mode transitions requested by whichever mode is active.
    if (sleepRequested) {
        sleepRequested = false;
        modeEngine.switchTo(MODE_SLEEP);
    }
    if (wakeRequested) {
        wakeRequested = false;
        modeEngine.switchTo(MODE_EVA);
        // Whether EVA woke from a tap or on its own after several
        // hours, it comes up calm/stationary and only starts moving
        // again once the next real touch happens.
        behaviour.wakeCalm();
    }

    commsHub.update();
    modeEngine.update();

    // RoboEyes owns the OLED unless the active mode explicitly
    // takes it over (Clock UI, Sleep's periodic time display).
    Mode *active = modeEngine.getCurrent();
    if (!active || !active->ownsDisplay()) {
        roboEyes.update();
    }
}
