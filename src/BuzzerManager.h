#pragma once
/*
 * BuzzerManager.h
 * ---------------------------------------------------------
 * Non-blocking note-sequence player for the passive buzzer.
 * A "sequence" is a small list of (frequency, durationMs)
 * steps played back using millis() timing so the rest of the
 * system keeps running while a sound plays.
 *
 * Volume is controlled via the PWM duty-cycle cap (0-255).
 * Default is BUZZER_VOLUME from Config.h; can be changed at
 * runtime with setVolume() or the BLE command VOLUME <0-255>.
 * ---------------------------------------------------------
 */

#include <Arduino.h>
#include "Types.h"
#include "Config.h"

struct BuzzerNote {
    uint16_t frequencyHz; // 0 = silence/rest
    uint16_t durationMs;
};

class BuzzerManager {
public:
    BuzzerManager();

    void begin();

    // Call every loop() to advance the currently playing sequence.
    void update();

    void playSequence(const BuzzerNote *notes, uint8_t count, bool loop = false);
    void stop();
    bool isPlaying() const;

    void setMuted(bool mute) { muted = mute; if (muted) stop(); }
    bool isMuted() const { return muted; }

    // Convenience: emotion-linked reaction sounds & EMO pitch contours

    void playForEmotion(EvaEmotion emotion);
    void playChirpUp();
    void playChirpDown();
    void playPurr(bool loop = false);
    void playAnnoyed();
    void playStartled();
    void playCuriousHmm();
    void playYawn();

    void playAlarmBeep();
    void playClockChime();

    // Volume control: 0 = silent, 255 = maximum (50% PWM duty).
    void setVolume(uint8_t vol);
    uint8_t getVolume() const;

private:
    const BuzzerNote *sequence;
    uint8_t sequenceLength;
    uint8_t currentIndex;
    unsigned long stepStartTime;
    bool playing;
    bool looping;
    bool muted;
    uint8_t volume;   // duty-cycle cap; set by setVolume()

    void startStep(uint8_t index);
};
