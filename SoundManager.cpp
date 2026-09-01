#include "SoundManager.h"
#include "Logger.h"

SoundManager::SoundManager()
    : lastPollTime(0),
      baseline(1600.0f),
      lastPeakTime(0),
      lastEventTime(0),
      lastLevel(0),
      peakCount(0),
      lastEvent(SoundEvent::NONE) {
    for (int i = 0; i < MIC_MUSIC_MIN_EVENTS; i++) peakTimestamps[i] = 0;
}

void SoundManager::begin() {
    pinMode(PIN_MIC_ANALOG, INPUT);
    // Seed baseline with a short burst of real ambient readings.
    long sum = 0;
    const int samples = 40;
    for (int i = 0; i < samples; i++) {
        sum += analogRead(PIN_MIC_ANALOG);
        delayMicroseconds(500); // one-time boot settling, not runtime behaviour
    }
    baseline = (float)sum / samples;
    EVA_LOGF("[SoundManager] baseline seeded at %.1f\n", baseline);
}

bool SoundManager::withinCooldown(unsigned long now) const {
    return (now - lastEventTime) < MIC_EVENT_COOLDOWN_MS;
}

void SoundManager::registerPeak(unsigned long now) {
    // Shift window and append.
    for (int i = 0; i < MIC_MUSIC_MIN_EVENTS - 1; i++) {
        peakTimestamps[i] = peakTimestamps[i + 1];
    }
    peakTimestamps[MIC_MUSIC_MIN_EVENTS - 1] = now;
    if (peakCount < MIC_MUSIC_MIN_EVENTS) peakCount++;
}

void SoundManager::update() {
    unsigned long now = millis();
    if (now - lastPollTime < MIC_POLL_INTERVAL_MS) return;
    lastPollTime = now;

    uint16_t raw = analogRead(PIN_MIC_ANALOG);
    lastLevel = raw;
    lastEvent = SoundEvent::NONE;

    bool isPeak = raw > (baseline * MIC_LOUD_RATIO);

    // Keep the baseline drifting slowly with ambient (quiet) levels
    // only, so a loud room doesn't get "normalised" away instantly.
    if (!isPeak) {
        baseline = (baseline * (1.0f - MIC_BASELINE_ALPHA)) + (raw * MIC_BASELINE_ALPHA);
    }

    if (!isPeak || withinCooldown(now)) {
        return;
    }

    // We have a fresh, de-bounced peak.
    unsigned long gapFromLastPeak = now - lastPeakTime;
    registerPeak(now);
    lastPeakTime = now;
    lastEventTime = now;

    // Music-like: many peaks inside a rolling multi-second window.
    if (peakCount >= MIC_MUSIC_MIN_EVENTS) {
        unsigned long windowSpan = peakTimestamps[MIC_MUSIC_MIN_EVENTS - 1] - peakTimestamps[0];
        if (windowSpan <= MIC_MUSIC_WINDOW_MS) {
            lastEvent = SoundEvent::MUSIC_LIKE;
            return;
        }
    }

    // Clap: second peak arriving shortly (but not instantly) after
    // the previous one.
    if (gapFromLastPeak >= MIC_CLAP_MIN_GAP_MS && gapFromLastPeak <= MIC_CLAP_MAX_GAP_MS) {
        lastEvent = SoundEvent::CLAP;
        return;
    }

    // Otherwise: an isolated sharp peak (knock, bang, door, etc).
    lastEvent = SoundEvent::LOUD_NOISE;
}

SoundEvent SoundManager::getEvent() {
    SoundEvent ev = lastEvent;
    lastEvent = SoundEvent::NONE; // consume-once: see header comment
    return ev;
}

uint16_t SoundManager::getLastLevel() const {
    return lastLevel;
}
