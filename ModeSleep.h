#pragma once
/*
 * ModeSleep.h
 * ---------------------------------------------------------
 * Mode 4 - Sleep. Entered from a long touch hold (Mode 1/2)
 * or automatically once EVA has been idle long enough.
 *
 *   Sleep -> (every 5 min) show time for up to 1 min -> Sleep
 *   Tap during sleep                    -> Wake -> Mode 1 (calm)
 *   Random 6-8h with no tap at all      -> Wake -> Mode 1 (calm)
 *
 * "Wake -> Mode 1 (calm)": whichever way EVA wakes, it comes up
 * stationary (behaviour.wakeCalm() is called by EVA.ino when it
 * sees wakeRequested) and only starts moving again once the next
 * real touch happens - it should not immediately drive off right
 * after waking on its own.
 *
 * Eye rendering while asleep: RoboEyes' own autoblinker and idle
 * (random-look) animations are DISABLED for the duration of deep
 * sleep. Leaving them on was the cause of the "closes, then a few
 * seconds later flashes back to a normal tired-open look" bug -
 * autoblink/idle were periodically re-opening/moving the eyes out
 * from under the closed state on their own. With both disabled,
 * eyes.close() (TIRED mood + shut eyelids -> a slim resting line)
 * stays put and stable until this mode explicitly re-opens them
 * for the periodic time display or on waking.
 * ---------------------------------------------------------
 */

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Mode.h"
#include "Types.h"
#include "Config.h"
#include "ClockService.h"
#include "TouchManager.h"

template <typename EmotionT, typename EyesT>
class ModeSleep : public Mode {
public:
    ModeSleep(EmotionT &emotionRef, EyesT &eyesRef,
              Adafruit_SSD1306 &displayRef,
              ClockService &clockRef, TouchManager &touchRef,
              bool &wakeRequestFlag)
        : emotion(emotionRef), eyes(eyesRef), display(displayRef),
          clock(clockRef), touch(touchRef), wakeRequested(wakeRequestFlag),
          showingTime(false), lastTimeShow(0), timeShowStarted(0),
          sleepEnteredAt(0), autoWakeAfterMs(0),
          lastBlinkAt(0), nextSlowMoveAt(0) {}

    void enter() override {
        emotion.setSleepy();

        // Sleep keeps the eyes closed and settled. The occasional eye
        // movement is allowed via slow idle drift, but blinking is disabled
        // so the face remains calm and restful.
        eyes.setAutoblinker(OFF, 0, 0);
        eyes.setIdleMode(ON, 18, 24);
        eyes.close();

        showingTime = false;
        lastTimeShow = millis(); // first periodic peek happens after one interval
        sleepEnteredAt = millis();
        autoWakeAfterMs = random(SLEEP_AUTO_WAKE_MIN_MS, SLEEP_AUTO_WAKE_MAX_MS);
        lastBlinkAt = millis();
        nextSlowMoveAt = millis() + random(8000UL, 18000UL);
        wakeRequested = false;
    }

    void update() override {
        touch.update();

        unsigned long now = millis();

        if (!showingTime && now >= nextSlowMoveAt) {
            // Allowed sleep-specific motion: very slow, subtle eye drift while
            // remaining closed. No blinking, no hard-open wake state.
            eyes.setIdleMode(ON, 18, 24);
            eyes.close();
            nextSlowMoveAt = now + random(8000UL, 18000UL);
        }

        if (touch.getEvent() == TouchEvent::TAP) {
            wakeRequested = true; // ModeEngine/EVA.ino handles the actual switch
            return;
        }

        if (now - sleepEnteredAt >= autoWakeAfterMs) {
            wakeRequested = true; // slept long enough on its own - wake up
            return;
        }

        if (!showingTime && (now - lastTimeShow >= SLEEP_TIME_DISPLAY_INTERVAL_MS)) {
            showingTime = true;
            timeShowStarted = now;
            eyes.setAutoblinker(OFF, 0, 0);
            eyes.setIdleMode(ON, 6, 10);
            eyes.open();
        }

        if (showingTime) {
            if (now - timeShowStarted >= SLEEP_TIME_DISPLAY_DURATION_MS) {
                showingTime = false;
                lastTimeShow = now;
                eyes.setAutoblinker(OFF, 0, 0);
                eyes.setIdleMode(ON, 18, 24);
                eyes.close();
            } else {
                renderTime();
            }
        }
    }

    void exit() override {
        eyes.setAutoblinker(OFF, 0, 0);
        eyes.setIdleMode(ON, 8, 12);
        eyes.open();
    }

    EvaMode id() const override { return MODE_SLEEP; }
    bool ownsDisplay() const override { return showingTime; }

private:
    EmotionT &emotion;
    EyesT &eyes;
    Adafruit_SSD1306 &display;
    ClockService &clock;
    TouchManager &touch;
    bool &wakeRequested;

    bool showingTime;
    unsigned long lastTimeShow;
    unsigned long timeShowStarted;
    unsigned long sleepEnteredAt;
    unsigned long autoWakeAfterMs;
    unsigned long lastBlinkAt;
    unsigned long nextSlowMoveAt;

    void renderTime() {
        display.clearDisplay();
        display.setTextColor(SSD1306_WHITE);
        display.setTextSize(2);
        display.setCursor(10, 24);
        display.println(clock.getTime12h());
        display.display();
    }
};
