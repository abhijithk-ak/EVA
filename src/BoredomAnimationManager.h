#pragma once
/*
 * BoredomAnimationManager.h
 * ---------------------------------------------------------
 * Non-blocking manager for OLED takeover animations
 * (dinosaur jump, pull-ups, snowfall, music party, car cruise)
 * using exact PROGMEM bitmap frame arrays imported from
 * animationsinos INO files (BoredomAnimationFrames.h).
 *
 * Includes Interactive Dino Jump Mini-Game Mode and global
 * enable/disable toggle via BLE/App.
 * ---------------------------------------------------------
 */

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "BoredomAnimationFrames.h"

enum IdleAnim {
    ANIM_DINO,
    ANIM_PULLUPS,
    ANIM_MUSIC,
    ANIM_SNOW,
    ANIM_CAR
};

class BoredomAnimationManager {
public:
    BoredomAnimationManager()
        : current(ANIM_DINO),
          playing(false),
          animEnabled(true),
          animStart(0),
          lastFrameTime(0),
          frameIndex(0),
          currentDuration(15000),
          gameActive(false),
          dinoJumping(false),
          dinoY(48),
          dinoVelocity(0),
          gameScore(0),
          cactusX(128) {}

    bool isPlaying() const { return playing || gameActive; }
    bool isEnabled() const { return animEnabled; }

    void setEnabled(bool enabled) {
        animEnabled = enabled;
        if (!animEnabled && playing) {
            playing = false;
        }
    }

    void trigger(IdleAnim anim, unsigned long durationMs = 0) {
        if (!animEnabled && anim != ANIM_MUSIC) return; // Allow manual triggers or music
        current = anim;
        playing = true;
        gameActive = false;
        animStart = millis();
        lastFrameTime = millis();
        frameIndex = 0;
        if (durationMs > 0) {
            currentDuration = durationMs;
        } else {
            currentDuration = random(10000UL, 25000UL); // Dynamic 10s to 25s duration
        }
    }

    void stop() {
        playing = false;
        gameActive = false;
    }

    // ---------------- Interactive Dino Game Mode ----------------
    void startDinoGame() {
        gameActive = true;
        playing = false;
        dinoY = 48;
        dinoJumping = false;
        dinoVelocity = 0;
        gameScore = 0;
        cactusX = 128;
        lastFrameTime = millis();
        animStart = millis();
    }

    bool isGameActive() const { return gameActive; }

    void jumpDino() {
        if (gameActive && !dinoJumping) {
            dinoJumping = true;
            dinoVelocity = -8; // Upward jump impulse
        }
    }

    // Called INSTEAD of eyes.update() while playing == true or gameActive == true
    void update(Adafruit_SSD1306& display) {
        if (!playing && !gameActive) return;
        unsigned long now = millis();

        if (gameActive) {
            if (now - lastFrameTime >= 40) { // 25 FPS game loop
                lastFrameTime = now;
                updateAndRenderGame(display);
            }
            return;
        }

        if (now - animStart > currentDuration) {
            playing = false; // Hand control back to RoboEyes
            return;
        }

        if (current == ANIM_DINO) {
            if (now - lastFrameTime >= 40) { // 25 FPS procedural dino animation loop
                lastFrameTime = now;
                updateAndRenderDinoAnim(display);
            }
            return;
        }

        if (now - lastFrameTime >= 80) { // ~12.5 FPS frame playback
            lastFrameTime = now;
            renderBitmapFrame(display);
            frameIndex++;
        }
    }

private:
    IdleAnim current;
    bool playing;
    bool animEnabled;
    unsigned long animStart;
    unsigned long lastFrameTime;
    int frameIndex;
    unsigned long currentDuration;

    // Dino Game & Animation State
    bool gameActive;
    bool dinoJumping;
    int dinoY;
    int dinoVelocity;
    int gameScore;
    int cactusX;

    void updateAndRenderDinoAnim(Adafruit_SSD1306& display) {
        display.clearDisplay();

        // Auto-jump logic for takeover animation mode
        int dinoX = 20;
        if (!dinoJumping && cactusX >= dinoX + 10 && cactusX <= dinoX + 35) {
            dinoJumping = true;
            dinoVelocity = -7; // Jump over cactus
        }

        // Physics update
        if (dinoJumping) {
            dinoY += dinoVelocity;
            dinoVelocity += 1; // Gravity
            if (dinoY >= 48) {
                dinoY = 48;
                dinoJumping = false;
                dinoVelocity = 0;
            }
        }

        // Cactus scrolling
        cactusX -= 4;
        if (cactusX < -15) {
            cactusX = 128 + random(10, 45);
        }

        // Background cloud scrolling decoration
        int cloudX = (128 - (int)((millis() / 50) % 170));
        if (cloudX > -30 && cloudX < 128) {
            display.drawRoundRect(cloudX, 8, 18, 6, 3, SSD1306_WHITE);
            display.drawRoundRect(cloudX + 5, 4, 10, 6, 3, SSD1306_WHITE);
        }

        // Ground line
        display.drawFastHLine(0, 56, 128, SSD1306_WHITE);

        // Ground motion dots for speed effect
        int dotOffset = (millis() / 20) % 16;
        for (int x = dotOffset; x < 128; x += 16) {
            display.drawPixel(x, 59, SSD1306_WHITE);
        }

        // Draw Cacti
        display.fillRect(cactusX, 44, 4, 12, SSD1306_WHITE);
        display.fillRect(cactusX - 2, 48, 8, 2, SSD1306_WHITE);

        // Draw Dino
        display.fillRect(dinoX - 4, dinoY - 8, 4, 4, SSD1306_WHITE); // Tail
        display.fillRect(dinoX, dinoY - 14, 12, 14, SSD1306_WHITE); // Body
        display.fillRect(dinoX + 6, dinoY - 20, 10, 8, SSD1306_WHITE); // Head
        display.drawPixel(dinoX + 12, dinoY - 18, SSD1306_BLACK); // Eye
        display.drawFastHLine(dinoX + 12, dinoY - 14, 4, SSD1306_BLACK); // Mouth
        display.fillRect(dinoX + 8, dinoY - 10, 3, 2, SSD1306_WHITE); // Tiny arm

        // Legs animation
        if (!dinoJumping) {
            if ((millis() / 100) % 2 == 0) {
                display.fillRect(dinoX + 2, dinoY, 3, 5, SSD1306_WHITE);
                display.drawFastVLine(dinoX + 8, dinoY, 2, SSD1306_WHITE);
            } else {
                display.drawFastVLine(dinoX + 2, dinoY, 2, SSD1306_WHITE);
                display.fillRect(dinoX + 8, dinoY, 3, 5, SSD1306_WHITE);
            }
        } else {
            display.drawFastVLine(dinoX + 2, dinoY, 3, SSD1306_WHITE);
            display.drawFastVLine(dinoX + 8, dinoY - 2, 3, SSD1306_WHITE);
        }

        // Header Title
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(38, 2);
        display.print("DINO RUN");

        display.display();
    }

    void updateAndRenderGame(Adafruit_SSD1306& display) {
        display.clearDisplay();

        // Physics update
        if (dinoJumping) {
            dinoY += dinoVelocity;
            dinoVelocity += 1; // Gravity
            if (dinoY >= 48) {
                dinoY = 48;
                dinoJumping = false;
                dinoVelocity = 0;
            }
        }

        // Cactus movement
        cactusX -= 4;
        if (cactusX < -10) {
            cactusX = 128 + random(0, 40);
            gameScore += 10;
        }

        // Ground line
        display.drawFastHLine(0, 56, 128, SSD1306_WHITE);

        // Draw Cacti
        display.fillRect(cactusX, 44, 4, 12, SSD1306_WHITE);
        display.fillRect(cactusX - 2, 48, 8, 2, SSD1306_WHITE);

        // Draw Dino
        int dinoX = 20;
        display.fillRect(dinoX - 4, dinoY - 8, 4, 4, SSD1306_WHITE); // Tail
        display.fillRect(dinoX, dinoY - 14, 12, 14, SSD1306_WHITE); // Body
        display.fillRect(dinoX + 6, dinoY - 20, 10, 8, SSD1306_WHITE); // Head
        display.drawPixel(dinoX + 12, dinoY - 18, SSD1306_BLACK); // Eye
        display.drawFastHLine(dinoX + 12, dinoY - 14, 4, SSD1306_BLACK); // Mouth
        display.fillRect(dinoX + 8, dinoY - 10, 3, 2, SSD1306_WHITE); // Arm
        if (!dinoJumping) {
            if ((millis() / 100) % 2 == 0) {
                display.fillRect(dinoX + 2, dinoY, 3, 5, SSD1306_WHITE);
                display.drawFastVLine(dinoX + 8, dinoY, 2, SSD1306_WHITE);
            } else {
                display.drawFastVLine(dinoX + 2, dinoY, 2, SSD1306_WHITE);
                display.fillRect(dinoX + 8, dinoY, 3, 5, SSD1306_WHITE);
            }
        } else {
            display.drawFastVLine(dinoX + 2, dinoY, 3, SSD1306_WHITE);
            display.drawFastVLine(dinoX + 8, dinoY - 2, 3, SSD1306_WHITE);
        }

        // Collision Check
        if (cactusX >= dinoX - 4 && cactusX <= dinoX + 10 && dinoY >= 42) {
            // Game Over flash
            display.setTextSize(1);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(35, 20);
            display.print("GAME OVER!");
            display.setCursor(30, 32);
            display.print("SCORE: ");
            display.print(gameScore);
            display.display();
            delay(1500);
            gameActive = false;
            return;
        }

        // Score display
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(80, 4);
        display.print("PTS:");
        display.print(gameScore);

        display.display();
    }

    void renderBitmapFrame(Adafruit_SSD1306& display) {
        display.clearDisplay();

        const uint8_t* const* frameTable = nullptr;
        uint8_t count = 0;

        switch (current) {
            case ANIM_DINO:
                frameTable = anim_dino_frames;
                count = DINO_FRAME_COUNT;
                break;
            case ANIM_PULLUPS:
                frameTable = anim_pullups_frames;
                count = PULLUPS_FRAME_COUNT;
                break;
            case ANIM_MUSIC:
                frameTable = anim_music_frames;
                count = MUSIC_FRAME_COUNT;
                break;
            case ANIM_SNOW:
                frameTable = anim_snow_frames;
                count = SNOW_FRAME_COUNT;
                break;
            case ANIM_CAR:
                frameTable = anim_car_frames;
                count = CAR_FRAME_COUNT;
                break;
        }

        if (frameTable && count > 0) {
            int activeFrameIdx = frameIndex % count;
            const uint8_t* framePtr = frameTable[activeFrameIdx];
            if (framePtr) {
                display.drawBitmap(0, 0, framePtr, 128, 64, SSD1306_WHITE);
            }
        }

        display.display();
    }
};
