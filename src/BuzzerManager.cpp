#include "BuzzerManager.h"

// ---- Pre-defined emotional/reaction note sequences ----
static const BuzzerNote SEQ_HAPPY[]       = { {880, 90}, {1046, 90}, {1318, 120} };
static const BuzzerNote SEQ_VERY_HAPPY[]  = { {880, 80}, {988, 80}, {1174, 80}, {1396, 140} };
static const BuzzerNote SEQ_CURIOUS[]     = { {600, 70}, {900, 70} };
static const BuzzerNote SEQ_SCARED[]      = { {300, 60}, {0, 40}, {300, 60} };
static const BuzzerNote SEQ_LOL[]         = { {700, 60}, {0, 30}, {700, 60}, {0, 30}, {900, 100} };
static const BuzzerNote SEQ_ANGRY[]       = { {200, 150}, {0, 40}, {200, 150} };
static const BuzzerNote SEQ_SLEEPY[]      = { {500, 200}, {400, 200}, {300, 300} };
static const BuzzerNote SEQ_ANNOYED[]     = { {350, 90}, {250, 110} };
static const BuzzerNote SEQ_AFFECTIONATE[]= { {784, 120}, {988, 140}, {784, 120}, {988, 160} };
static const BuzzerNote SEQ_SHY[]         = { {440, 60} };
static const BuzzerNote SEQ_EXCITED[]     = { {988, 70}, {1318, 70}, {1760, 100} };
static const BuzzerNote SEQ_STARTLED[]    = { {1800, 70} };
static const BuzzerNote SEQ_CONFUSED[]    = { {550, 80}, {750, 80}, {550, 80}, {750, 80} };
static const BuzzerNote SEQ_PROUD[]       = { {1046, 80}, {1318, 80}, {2093, 130} };
static const BuzzerNote SEQ_SUSPICIOUS[]  = { {480, 150} };
static const BuzzerNote SEQ_SAD[]         = { {600, 150}, {450, 150}, {300, 250} };

static const BuzzerNote SEQ_ALARM[]       = {
    {880, 250}, {0, 120},
    {660, 260}, {0, 140},
    {880, 260}, {0, 140},
    {660, 260}, {0, 140},
    {980, 350}, {0, 150},
    {660, 400}, {0, 180}
};
static const BuzzerNote SEQ_CHIME[]       = { {1046, 100}, {1318, 100}, {1568, 150} };

static const BuzzerNote SOUND_CHIRP_UP_SEQ[]    = {{800, 80}, {1000, 80}, {1300, 100}};
static const BuzzerNote SOUND_CHIRP_DOWN_SEQ[]  = {{1000, 120}, {700, 120}, {400, 180}};
static const BuzzerNote SOUND_PURR_SEQ[]        = {{300, 150}, {320, 150}};
static const BuzzerNote SOUND_ANNOYED_SEQ[]     = {{200, 90}, {150, 90}};
static const BuzzerNote SOUND_STARTLED_SEQ[]    = {{1500, 60}};
static const BuzzerNote SOUND_CURIOUS_HMM_SEQ[] = {{600, 70}, {750, 70}};
static const BuzzerNote SOUND_YAWN_SEQ[]        = {{500, 200}, {400, 200}, {300, 300}};

BuzzerManager::BuzzerManager()
    : sequence(nullptr), sequenceLength(0), currentIndex(0),
      stepStartTime(0), playing(false), looping(false), muted(false), volume(BUZZER_VOLUME) {}


void BuzzerManager::begin() {
    ledcSetup(PWM_CH_BUZZER, 2000, PWM_RESOLUTION_BIT);
    ledcAttachPin(PIN_BUZZER, PWM_CH_BUZZER);
    ledcWrite(PWM_CH_BUZZER, 0);
    volume = BUZZER_VOLUME;
}

void BuzzerManager::startStep(uint8_t index) {
    currentIndex  = index;
    stepStartTime = millis();
    const BuzzerNote &n = sequence[index];
    if (n.frequencyHz > 0) {
        ledcSetup(PWM_CH_BUZZER, n.frequencyHz, PWM_RESOLUTION_BIT);
        ledcWrite(PWM_CH_BUZZER, volume >> 1);
    } else {
        ledcWrite(PWM_CH_BUZZER, 0);
    }
}

void BuzzerManager::playSequence(const BuzzerNote *notes, uint8_t count, bool loop) {
    if (muted || count == 0) return;
    sequence = notes;
    sequenceLength = count;
    playing = true;
    looping = loop;
    startStep(0);
}


void BuzzerManager::stop() {
    playing = false;
    looping = false;
    ledcWrite(PWM_CH_BUZZER, 0);
}

bool BuzzerManager::isPlaying() const {
    return playing;
}

void BuzzerManager::update() {
    if (!playing || !sequence) return;

    const BuzzerNote &n = sequence[currentIndex];
    if (millis() - stepStartTime >= n.durationMs) {
        uint8_t next = currentIndex + 1;
        if (next >= sequenceLength) {
            if (looping) {
                startStep(0);
            } else {
                stop();
            }
        } else {
            startStep(next);
        }
    }
}

void BuzzerManager::playChirpUp()    { playSequence(SOUND_CHIRP_UP_SEQ, sizeof(SOUND_CHIRP_UP_SEQ)/sizeof(BuzzerNote)); }
void BuzzerManager::playChirpDown()  { playSequence(SOUND_CHIRP_DOWN_SEQ, sizeof(SOUND_CHIRP_DOWN_SEQ)/sizeof(BuzzerNote)); }
void BuzzerManager::playPurr(bool loop) { playSequence(SOUND_PURR_SEQ, sizeof(SOUND_PURR_SEQ)/sizeof(BuzzerNote), loop); }
void BuzzerManager::playAnnoyed()    { playSequence(SOUND_ANNOYED_SEQ, sizeof(SOUND_ANNOYED_SEQ)/sizeof(BuzzerNote)); }
void BuzzerManager::playStartled()   { playSequence(SOUND_STARTLED_SEQ, sizeof(SOUND_STARTLED_SEQ)/sizeof(BuzzerNote)); }
void BuzzerManager::playCuriousHmm() { playSequence(SOUND_CURIOUS_HMM_SEQ, sizeof(SOUND_CURIOUS_HMM_SEQ)/sizeof(BuzzerNote)); }
void BuzzerManager::playYawn()       { playSequence(SOUND_YAWN_SEQ, sizeof(SOUND_YAWN_SEQ)/sizeof(BuzzerNote)); }

void BuzzerManager::playForEmotion(EvaEmotion emotion) {
    switch (emotion) {
        case EVA_HAPPY:       playChirpUp(); break;
        case EVA_VERY_HAPPY:  playSequence(SEQ_VERY_HAPPY, sizeof(SEQ_VERY_HAPPY) / sizeof(BuzzerNote)); break;
        case EVA_CURIOUS:     playCuriousHmm(); break;
        case EVA_SCARED:      playSequence(SEQ_SCARED, sizeof(SEQ_SCARED) / sizeof(BuzzerNote)); break;
        case EVA_LOL:         playSequence(SEQ_LOL, sizeof(SEQ_LOL) / sizeof(BuzzerNote)); break;
        case EVA_ANGRY:       playAnnoyed(); break;
        case EVA_SLEEPY:      playYawn(); break;
        case EVA_ANNOYED:     playAnnoyed(); break;
        case EVA_AFFECTIONATE:playPurr(false); break;
        case EVA_SHY:         playSequence(SEQ_SHY, sizeof(SEQ_SHY) / sizeof(BuzzerNote)); break;
        case EVA_EXCITED:     playSequence(SEQ_EXCITED, sizeof(SEQ_EXCITED) / sizeof(BuzzerNote)); break;
        case EVA_STARTLED:    playStartled(); break;
        case EVA_CONFUSED:    playSequence(SEQ_CONFUSED, sizeof(SEQ_CONFUSED) / sizeof(BuzzerNote)); break;
        case EVA_PROUD:       playSequence(SEQ_PROUD, sizeof(SEQ_PROUD) / sizeof(BuzzerNote)); break;
        case EVA_SUSPICIOUS:  playSequence(SEQ_SUSPICIOUS, sizeof(SEQ_SUSPICIOUS) / sizeof(BuzzerNote)); break;
        case EVA_SAD:         playChirpDown(); break;
        case EVA_BORED:       /* silent blip */ break;
        case EVA_NEUTRAL:     /* silence */ break;
    }
}

void BuzzerManager::playAlarmBeep() {
    playSequence(SEQ_ALARM, sizeof(SEQ_ALARM) / sizeof(BuzzerNote));
}

void BuzzerManager::playClockChime() {
    playSequence(SEQ_CHIME, sizeof(SEQ_CHIME) / sizeof(BuzzerNote));
}

void BuzzerManager::setVolume(uint8_t vol) {
    volume = vol;
}

uint8_t BuzzerManager::getVolume() const {
    return volume;
}

