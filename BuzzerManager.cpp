#include "BuzzerManager.h"

// ---- Pre-defined emotional/reaction note sequences ----
static const BuzzerNote SEQ_HAPPY[]      = { {880, 90}, {1046, 90}, {1318, 120} };
static const BuzzerNote SEQ_VERY_HAPPY[] = { {880, 80}, {988, 80}, {1174, 80}, {1396, 140} };
static const BuzzerNote SEQ_CURIOUS[]    = { {600, 70}, {900, 70} };
static const BuzzerNote SEQ_SCARED[]     = { {300, 60}, {0, 40}, {300, 60} };
static const BuzzerNote SEQ_LOL[]        = { {700, 60}, {0, 30}, {700, 60}, {0, 30}, {900, 100} };
static const BuzzerNote SEQ_ANGRY[]      = { {200, 150}, {0, 40}, {200, 150} };
static const BuzzerNote SEQ_SLEEPY[]     = { {500, 200}, {400, 200}, {300, 300} };
static const BuzzerNote SEQ_ALARM[]      = {
    {880, 250}, {0, 120},
    {660, 260}, {0, 140},
    {880, 260}, {0, 140},
    {660, 260}, {0, 140},
    {980, 350}, {0, 150},
    {660, 400}, {0, 180}
};
static const BuzzerNote SEQ_CHIME[]      = { {1046, 100}, {1318, 100}, {1568, 150} };

BuzzerManager::BuzzerManager()
    : sequence(nullptr), sequenceLength(0), currentIndex(0),
      stepStartTime(0), playing(false) {}

void BuzzerManager::begin() {
    // IMPORTANT: do NOT use Arduino's tone()/noTone() here. On ESP32,
    // tone() silently grabs its own LEDC PWM channel behind the scenes,
    // independent of the channels MovementEngine has already claimed for
    // the motors (PWM_CH_LF/LB/RB/RF). If tone() lands on a channel a
    // motor pin is already attached to, playing a tone re-programs that
    // channel's frequency/duty and can drive straight out to a motor pin
    // - which looks exactly like "the motor twitches whenever the buzzer
    // sounds." Giving the buzzer its own dedicated, explicitly-numbered
    // channel (PWM_CH_BUZZER, defined in Config.h, distinct from every
    // motor channel) avoids the collision entirely.
    ledcSetup(PWM_CH_BUZZER, 2000, PWM_RESOLUTION_BIT);
    ledcAttachPin(PIN_BUZZER, PWM_CH_BUZZER);
    ledcWrite(PWM_CH_BUZZER, 0);
}

void BuzzerManager::startStep(uint8_t index) {
    currentIndex = index;
    stepStartTime = millis();
    const BuzzerNote &n = sequence[index];
    if (n.frequencyHz > 0) {
        ledcWriteTone(PWM_CH_BUZZER, n.frequencyHz);
    } else {
        ledcWriteTone(PWM_CH_BUZZER, 0);
    }
}

void BuzzerManager::playSequence(const BuzzerNote *notes, uint8_t count) {
    if (count == 0) return;
    sequence = notes;
    sequenceLength = count;
    playing = true;
    startStep(0);
}

void BuzzerManager::stop() {
    playing = false;
    ledcWriteTone(PWM_CH_BUZZER, 0);
}

bool BuzzerManager::isPlaying() const {
    return playing;
}

void BuzzerManager::update() {
    if (!playing) return;

    const BuzzerNote &n = sequence[currentIndex];
    if (millis() - stepStartTime >= n.durationMs) {
        uint8_t next = currentIndex + 1;
        if (next >= sequenceLength) {
            stop();
        } else {
            startStep(next);
        }
    }
}

void BuzzerManager::playForEmotion(EvaEmotion emotion) {
    switch (emotion) {
        case EVA_HAPPY:      playSequence(SEQ_HAPPY, sizeof(SEQ_HAPPY) / sizeof(BuzzerNote)); break;
        case EVA_VERY_HAPPY: playSequence(SEQ_VERY_HAPPY, sizeof(SEQ_VERY_HAPPY) / sizeof(BuzzerNote)); break;
        case EVA_CURIOUS:    playSequence(SEQ_CURIOUS, sizeof(SEQ_CURIOUS) / sizeof(BuzzerNote)); break;
        case EVA_SCARED:     playSequence(SEQ_SCARED, sizeof(SEQ_SCARED) / sizeof(BuzzerNote)); break;
        case EVA_LOL:        playSequence(SEQ_LOL, sizeof(SEQ_LOL) / sizeof(BuzzerNote)); break;
        case EVA_ANGRY:      playSequence(SEQ_ANGRY, sizeof(SEQ_ANGRY) / sizeof(BuzzerNote)); break;
        case EVA_SLEEPY:     playSequence(SEQ_SLEEPY, sizeof(SEQ_SLEEPY) / sizeof(BuzzerNote)); break;
        case EVA_NEUTRAL:    /* silence */ break;
    }
}

void BuzzerManager::playAlarmBeep() {
    playSequence(SEQ_ALARM, sizeof(SEQ_ALARM) / sizeof(BuzzerNote));
}

void BuzzerManager::playClockChime() {
    playSequence(SEQ_CHIME, sizeof(SEQ_CHIME) / sizeof(BuzzerNote));
}
