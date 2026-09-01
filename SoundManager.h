#pragma once
/*
 * SoundManager.h
 * ---------------------------------------------------------
 * Interprets the MAX9814 microphone amplifier's analog
 * envelope output. This is environmental awareness ONLY —
 * there is no speech recognition, and this class does not
 * attempt to be a real audio classifier. It uses a dynamic
 * noise-floor baseline plus simple peak/gap timing to guess
 * at a small set of coarse events:
 *
 *   CLAP        - one or two sharp peaks close together
 *   LOUD_NOISE  - a single sharp, large peak (knock/bang/door)
 *   MUSIC_LIKE  - sustained, repeated peaks over a few seconds
 *
 * False positives/negatives are expected and acceptable —
 * the design intent is "EVA notices something happened
 * nearby", not accurate classification. As with all sensors
 * in this project, events are reported for BehaviourEngine
 * to interpret; SoundManager never drives outputs itself.
 * ---------------------------------------------------------
 */

#include <Arduino.h>
#include "Types.h"
#include "Config.h"

class SoundManager {
public:
    SoundManager();

    void begin();

    // Call every loop(). Internally rate-limited to
    // MIC_POLL_INTERVAL_MS.
    void update();

    // Consume-once - see TouchManager::getEvent() for why this
    // can't be a plain getter (update() is rate-limited internally,
    // loop() is not, so a plain getter would replay the same event
    // on every loop() iteration until the next real poll).
    SoundEvent getEvent();
    uint16_t getLastLevel() const;

private:
    unsigned long lastPollTime;
    float baseline;

    unsigned long lastPeakTime;
    unsigned long lastEventTime;
    uint16_t lastLevel;

    // Rolling window for music-like detection
    unsigned long peakTimestamps[MIC_MUSIC_MIN_EVENTS];
    uint8_t peakCount;

    SoundEvent lastEvent;

    void registerPeak(unsigned long now);
    bool withinCooldown(unsigned long now) const;
};
