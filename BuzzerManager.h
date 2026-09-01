#pragma once
/*
 * BuzzerManager.h
 * ---------------------------------------------------------
 * Non-blocking note-sequence player for the passive buzzer.
 * A "sequence" is a small list of (frequency, durationMs)
 * steps played back using millis() timing so the rest of the
 * system keeps running while a sound plays.
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

    void playSequence(const BuzzerNote *notes, uint8_t count);
    void stop();
    bool isPlaying() const;

    // Convenience: emotion-linked reaction sounds.
    void playForEmotion(EvaEmotion emotion);
    void playAlarmBeep();
    void playClockChime();

private:
    const BuzzerNote *sequence;
    uint8_t sequenceLength;
    uint8_t currentIndex;
    unsigned long stepStartTime;
    bool playing;

    void startStep(uint8_t index);
};
