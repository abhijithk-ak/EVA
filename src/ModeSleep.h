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
    enum PeekType {
        PEEK_NONE,
        PEEK_CLOCK,
        PEEK_ANIM
    };

    ModeSleep(EmotionT &emotionRef, EyesT &eyesRef,
              Adafruit_SSD1306 &displayRef,
              ClockService &clockRef, TouchManager &touchRef,
              bool &wakeRequestFlag)
        : emotion(emotionRef), eyes(eyesRef), display(displayRef),
          clock(clockRef), touch(touchRef), wakeRequested(wakeRequestFlag),
          activePeek(PEEK_NONE), lastClockShow(0), lastAnimShow(0), peekStarted(0),
          sleepEnteredAt(0), autoWakeAfterMs(0),
          peekAnimIndex(0), nextSlowMoveAt(0) {}

    void enter() override {
        emotion.setSleepy();

        // Sleep keeps eyes closed and perfectly still.
        // setIdleMode MUST be OFF — ON causes random eye movement that
        // fights with eyes.close() and creates the "flashing open" bug.
        eyes.setAutoblinker(OFF, 0, 0);
        eyes.setIdleMode(OFF, 0, 0);
        eyes.close();

        activePeek = PEEK_NONE;
        unsigned long now = millis();
        lastClockShow = now;
        lastAnimShow = now;
        sleepEnteredAt = now;
        autoWakeAfterMs = random(SLEEP_AUTO_WAKE_MIN_MS, SLEEP_AUTO_WAKE_MAX_MS);
        nextSlowMoveAt = now + random(8000UL, 18000UL);
        wakeRequested = false;
    }

    void update() override {
        touch.update();

        unsigned long now = millis();

        if (activePeek == PEEK_NONE && now >= nextSlowMoveAt) {
            // Keep idle mode OFF — we only want eyes fully closed, not moving
            eyes.setIdleMode(OFF, 0, 0);
            eyes.close();
            nextSlowMoveAt = now + random(8000UL, 18000UL);
        }

        if (touch.getEvent() == TouchEvent::TAP) {
            wakeRequested = true; // ModeEngine/EVA.ino handles switch to PET mode
            return;
        }

        if (now - sleepEnteredAt >= autoWakeAfterMs) {
            wakeRequested = true; // Auto-wake timer (1-2 hours)
            return;
        }

        // Schedule peeks when no peek is active
        if (activePeek == PEEK_NONE) {
            if (now - lastClockShow >= SLEEP_CLOCK_PEEK_INTERVAL_MS) {
                activePeek = PEEK_CLOCK;
                peekStarted = now;
                lastClockShow = now;
                eyes.setAutoblinker(OFF, 0, 0);
                eyes.open();
            } else if (now - lastAnimShow >= SLEEP_ANIM_PEEK_INTERVAL_MS) {
                activePeek = PEEK_ANIM;
                peekStarted = now;
                lastAnimShow = now;
                peekAnimIndex = random(0, 2);
                eyes.setAutoblinker(OFF, 0, 0);
                eyes.open();
            }
        }

        // Active peek rendering & completion check
        if (activePeek != PEEK_NONE) {
            if (now - peekStarted >= SLEEP_PEEK_DURATION_MS) {
                activePeek = PEEK_NONE;
                eyes.setAutoblinker(OFF, 0, 0);
                eyes.setIdleMode(OFF, 0, 0);  // back to fully closed, no movement
                eyes.close();
            } else {
                if (activePeek == PEEK_CLOCK) {
                    renderTime();
                } else if (activePeek == PEEK_ANIM) {
                    renderSleepAnimation();
                }
            }
        }
    }

    void exit() override {
        eyes.setAutoblinker(OFF, 0, 0);
        eyes.setIdleMode(ON, 8, 12);
        eyes.open();
    }

    EvaMode id() const override { return MODE_SLEEP; }
    bool ownsDisplay() const override { return activePeek != PEEK_NONE; }

private:
    EmotionT &emotion;
    EyesT &eyes;
    Adafruit_SSD1306 &display;
    ClockService &clock;
    TouchManager &touch;
    bool &wakeRequested;

    PeekType activePeek;
    unsigned long lastClockShow;
    unsigned long lastAnimShow;
    unsigned long peekStarted;
    unsigned long sleepEnteredAt;
    unsigned long autoWakeAfterMs;
    uint8_t peekAnimIndex;
    unsigned long nextSlowMoveAt;

    void renderTime() {
        display.clearDisplay();
        display.setTextColor(SSD1306_WHITE);
        display.setTextSize(2);
        display.setCursor(16, 24);
        display.println(clock.getTime12h());
        display.display();
    }

    void renderSleepAnimation() {
        display.clearDisplay();
        unsigned long frame = (millis() / 80);

        if (peekAnimIndex == 0) {
            // Snow Animation Peek
            for (int i = 0; i < 18; i++) {
                int x = (i * 13 + (frame * (i % 3 + 1))) % 128;
                int y = (i * 5 + frame * 2) % 64;
                display.drawPixel(x, y, SSD1306_WHITE);
            }
            display.fillTriangle(25, 42, 16, 58, 34, 58, SSD1306_WHITE);
            display.fillTriangle(100, 40, 92, 58, 108, 58, SSD1306_WHITE);
            display.drawFastHLine(0, 62, 128, SSD1306_WHITE);
        } else {
            // Dino Jump Peek
            int dinoY = (frame % 8 < 4) ? 36 : 48;
            display.fillRect(20, dinoY - 14, 12, 14, SSD1306_WHITE);
            display.fillRect(28, dinoY - 18, 8, 8, SSD1306_WHITE);
            display.drawPixel(32, dinoY - 16, SSD1306_BLACK);
            display.drawFastHLine(0, 52, 128, SSD1306_WHITE);
            display.fillRect(90, 40, 4, 12, SSD1306_WHITE);
        }
        display.display();
    }
};


