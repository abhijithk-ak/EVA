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
 * V3 Step 1: MAX9812 mic and all sound-related code removed.
 *            SoundManager deleted. SoundEvent deleted from Types.h.
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
TouchManager  touchSensor;

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
bool wakeRequested  = false;
bool evaRequested   = false;

// ---------------------------------------------------------
// Modes
// ---------------------------------------------------------
ModeEva<decltype(behaviour)>   modeEva(behaviour, spatialSensor, touchSensor, sleepRequested);
ModePet<decltype(behaviour)>   modePet(behaviour, touchSensor, sleepRequested, evaRequested);
ModeClock                      modeClock(display, clockService, buzzer, touchSensor, movement);
ModeSleep<decltype(emotion), decltype(roboEyes)> modeSleep(emotion, roboEyes, display, clockService, touchSensor, wakeRequested);
ModeRC<decltype(emotion)>      modeRC(emotion, movement);

ModeEngine modeEngine(&movement);


// ---------------------------------------------------------
// Remote control: one always-on Bluetooth Serial channel for
// mode switching, alarm/time commands, AND RC driving. See
// CommsHub.h for the protocol.
// ---------------------------------------------------------
CommsHub commsHub(modeEngine, clockService, buzzer, movement, display);

void setup() {
    EVA_LOG_BEGIN();
    EVA_LOGLN("\n[EVA] booting...");

    Wire.begin(EVA_I2C_SDA, EVA_I2C_SCL);

    if (!display.begin(SSD1306_SWITCHCAPVCC, EVA_OLED_I2C_ADDR)) {
        EVA_LOGLN("[EVA] SSD1306 init FAILED - halting");
        while (true) { delay(1000); }
    }

    roboEyes.begin(EVA_SCREEN_WIDTH, EVA_SCREEN_HEIGHT, 100);
    roboEyes.setAutoblinker(ON, 3, 2);
    roboEyes.setIdleMode(ON, 2, 2);

    movement.begin();
    light.begin();
    buzzer.begin();
    touchSensor.begin();
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
    commsHub.setBrightness(DISPLAY_BRIGHTNESS); // apply initial OLED brightness
    commsHub.setEmotionRunner(&behaviour); // connect BLE trick/emotion triggers
    modeClock.setCommsHub(&commsHub); // lets clock UI reflect BT connected status

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
        modeEngine.switchTo(MODE_PET); // Wakes to PET mode (no motor movement!)
        behaviour.wakeCalm();
    }
    if (evaRequested) {
        evaRequested = false;
        modeEngine.switchTo(MODE_EVA); // Transition to full EVA mode upon touch/petting
    }

    commsHub.update();
    modeEngine.update();

    // RoboEyes owns the OLED unless the active mode explicitly
    // takes it over (Clock UI, Sleep's periodic time display)
    // or BoredomAnimationManager is playing a takeover animation.
    Mode *active = modeEngine.getCurrent();
    if (!active || !active->ownsDisplay()) {
        if (behaviour.getBoredomAnim().isPlaying()) {
            behaviour.getBoredomAnim().update(display);
        } else {
            roboEyes.update();
        }
    }
}
