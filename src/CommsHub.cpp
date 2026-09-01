#include "CommsHub.h"
#include "Logger.h"

namespace {
    bool isMovementChar(char c) {
        switch (c) {
            case 'F': case 'B': case 'L': case 'R':
            case 'G': case 'I': case 'H': case 'J': case 'S':
                return true;
            default:
                return (c >= '0' && c <= '9');
        }
    }
}

CommsHub::CommsHub(ModeEngine &modeEngineRef, ClockService &clockRef,
                   BuzzerManager &buzzerRef, MovementEngine &movementRef,
                   Adafruit_SSD1306 &displayRef)
    : modeEngine(modeEngineRef), clock(clockRef), buzzer(buzzerRef),
      movement(movementRef), display(displayRef), emotionRunner(nullptr),
      rcSpeed(MOVE_DEFAULT_SPEED), lastRcCommandTime(0) {}

void CommsHub::begin() {
    bt.begin(BT_DEVICE_NAME);
    EVA_LOGF("[CommsHub] Bluetooth Serial '%s' ready\n", BT_DEVICE_NAME);
}

bool CommsHub::isConnected() {
    return bt.hasClient();
}

void CommsHub::setEmotionRunner(IEmotionRunner *runner) {
    emotionRunner = runner;
}

void CommsHub::setBrightness(uint8_t level) {
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(level);

    display.ssd1306_command(0xD9);
    if (level < 32) {
        display.ssd1306_command(0x11);
    } else if (level < 128) {
        display.ssd1306_command(0x22);
    } else {
        display.ssd1306_command(0xF1);
    }

    display.ssd1306_command(0xDB);
    if (level < 64) {
        display.ssd1306_command(0x00);
    } else if (level < 192) {
        display.ssd1306_command(0x20);
    } else {
        display.ssd1306_command(0x30);
    }
}

void CommsHub::update() {
    while (bt.available()) {
        handleByte((char)bt.read());
    }
    rcSafetyWatchdog();
}

void CommsHub::handleByte(char c) {
    if (c == '\n' || c == '\r') {
        if (lineBuffer.length() > 0) {
            executeTextCommand(lineBuffer);
            lineBuffer = "";
        }
        return;
    }

    if ((uint8_t)c < 32 || (uint8_t)c > 126) {
        return;
    }

    // In MODE_RC, execute movement characters immediately when lineBuffer is empty or single char
    if (modeEngine.getCurrentId() == MODE_RC && isMovementChar(c)) {
        if (lineBuffer.length() <= 1) {
            executeMovementChar(c);
            lineBuffer = "";
            return;
        }
    }


    lineBuffer += c;
    if (lineBuffer.length() > 40) {
        lineBuffer = "";
    }
}

void CommsHub::executeMovementChar(char c) {
    if (modeEngine.getCurrentId() != MODE_RC) return;

    lastRcCommandTime = millis();

    switch (c) {
        case 'F': movement.drive(MoveState::FORWARD, rcSpeed);      break;
        case 'B': movement.drive(MoveState::BACKWARD, rcSpeed);     break;
        case 'L': movement.drive(MoveState::PIVOT_LEFT, rcSpeed);   break;
        case 'R': movement.drive(MoveState::PIVOT_RIGHT, rcSpeed);  break;
        case 'G': movement.drive(MoveState::CURVE_LEFT, rcSpeed);   break;
        case 'I': movement.drive(MoveState::CURVE_RIGHT, rcSpeed);  break;
        case 'H': movement.drive(MoveState::PIVOT_LEFT, rcSpeed);   break;
        case 'J': movement.drive(MoveState::PIVOT_RIGHT, rcSpeed);  break;
        case 'S': movement.stop(); break;
        default:
            if (c >= '0' && c <= '9') {
                rcSpeed = map(c - '0', 0, 9, 120, 255);
            }
            break;
    }
}

void CommsHub::executeTextCommand(String cmd) {
    cmd.trim();
    if (cmd.length() == 0) return;

    String upper = cmd;
    upper.toUpperCase();

    // ---- Mode switching ----
    if      (upper == "MODE EVA")   { modeEngine.switchTo(MODE_EVA); }
    else if (upper == "MODE PET")   { modeEngine.switchTo(MODE_PET); }
    else if (upper == "MODE CLOCK") { modeEngine.switchTo(MODE_CLOCK); }
    else if (upper == "MODE SLEEP") { modeEngine.switchTo(MODE_SLEEP); }
    else if (upper == "MODE RC")    { modeEngine.switchTo(MODE_RC); }

    // ---- Special Trick / Play Triggers ----
    else if (upper == "DANCE") {
        if (emotionRunner) emotionRunner->triggerDance();
        EVA_LOGLN("[CommsHub] BLE Trigger: DANCE");
    }
    else if (upper == "TRICK") {
        if (emotionRunner) emotionRunner->triggerTrick();
        EVA_LOGLN("[CommsHub] BLE Trigger: TRICK");
    }
    else if (upper == "SCARE" || upper == "CARE" || upper == "BOO") {
        if (emotionRunner) emotionRunner->triggerScare();
        EVA_LOGLN("[CommsHub] BLE Trigger: SCARE/CARE/BOO");
    }
    else if (upper == "RANDOM" || upper == "RANDOM_EMOTION" || upper == "ANDOM") {
        if (emotionRunner) emotionRunner->triggerRandom();
        EVA_LOGLN("[CommsHub] BLE Trigger: RANDOM");
    }

    else if (upper.startsWith("EMOTION ")) {
        String emoName = upper.substring(8);
        emoName.trim();
        EvaEmotion emo = EVA_HAPPY;
        if (emoName == "NEUTRAL") emo = EVA_NEUTRAL;
        else if (emoName == "HAPPY") emo = EVA_HAPPY;
        else if (emoName == "VERY_HAPPY" || emoName == "GLEE") emo = EVA_VERY_HAPPY;
        else if (emoName == "CURIOUS") emo = EVA_CURIOUS;
        else if (emoName == "SCARED") emo = EVA_SCARED;
        else if (emoName == "LOL") emo = EVA_LOL;
        else if (emoName == "ANGRY" || emoName == "FURIOUS") emo = EVA_ANGRY;
        else if (emoName == "SLEEPY") emo = EVA_SLEEPY;
        else if (emoName == "ANNOYED") emo = EVA_ANNOYED;
        else if (emoName == "AFFECTIONATE") emo = EVA_AFFECTIONATE;
        else if (emoName == "SHY") emo = EVA_SHY;
        else if (emoName == "EXCITED") emo = EVA_EXCITED;
        else if (emoName == "STARTLED") emo = EVA_STARTLED;
        else if (emoName == "CONFUSED") emo = EVA_CONFUSED;
        else if (emoName == "BORED") emo = EVA_BORED;
        else if (emoName == "PROUD") emo = EVA_PROUD;
        else if (emoName == "SUSPICIOUS") emo = EVA_SUSPICIOUS;
        else if (emoName == "SAD") emo = EVA_SAD;
        else if (emoName == "SKEPTIC") emo = EVA_SKEPTIC;
        else if (emoName == "WORRIED") emo = EVA_WORRIED;
        else if (emoName == "FOCUSED") emo = EVA_FOCUSED;
        else if (emoName == "SURPRISED") emo = EVA_SURPRISED;
        else if (emoName == "FRUSTRATED") emo = EVA_FRUSTRATED;
        else if (emoName == "UNIMPRESSED") emo = EVA_UNIMPRESSED;
        else if (emoName == "SQUINT") emo = EVA_SQUINT;
        else if (emoName == "FURIOUS") emo = EVA_FURIOUS;
        else if (emoName == "AWE") emo = EVA_AWE;

        if (emotionRunner) emotionRunner->triggerEmotion(emo);
        EVA_LOGF("[CommsHub] BLE Trigger EMOTION: %s\n", emoName.c_str());
    }

    else if (upper.startsWith("ANIM ") || upper.startsWith("ANIMATION ")) {
        String animStr = upper.substring(upper.indexOf(' ') + 1);
        animStr.trim();
        if (animStr == "ON" || animStr == "ENABLE") {
            if (emotionRunner) emotionRunner->setAnimationsEnabled(true);
            EVA_LOGF("[CommsHub] Takeover Animations ENABLED\n");
        } else if (animStr == "OFF" || animStr == "DISABLE") {
            if (emotionRunner) emotionRunner->setAnimationsEnabled(false);
            EVA_LOGF("[CommsHub] Takeover Animations DISABLED\n");
        } else {
            uint8_t animId = 0;
            if (animStr == "DINO" || animStr == "DINOJUMP") animId = 0;
            else if (animStr == "PULLUPS" || animStr == "WORKOUT") animId = 1;
            else if (animStr == "MUSIC" || animStr == "PARTY") animId = 2;
            else if (animStr == "SNOW" || animStr == "SNOWFALL") animId = 3;
            else if (animStr == "CAR" || animStr == "CRUISE") animId = 4;

            if (emotionRunner) emotionRunner->triggerAnimation(animId);
            EVA_LOGF("[CommsHub] BLE Trigger ANIMATION: %s (%d)\n", animStr.c_str(), animId);
        }
    }
    else if (upper.startsWith("GAME ")) {
        String gameStr = upper.substring(5);
        gameStr.trim();
        if (gameStr == "DINO" || gameStr == "START") {
            if (emotionRunner) emotionRunner->startDinoGame();
            EVA_LOGF("[CommsHub] Dino Mini Game Started!\n");
        } else if (gameStr == "JUMP") {
            if (emotionRunner) emotionRunner->jumpDino();
            EVA_LOGF("[CommsHub] Dino Jump Action\n");
        }
    }
    else if (upper == "DANCE" || upper == "TRICK DANCE") {
        if (emotionRunner) emotionRunner->triggerDance();
        EVA_LOGF("[CommsHub] BLE Trigger DANCE Routine\n");
    }

    // ---- Time: TIME HH:MM ----
    else if (upper.startsWith("TIME ")) {
        bool ok = clock.setManualTimeHHMM(cmd.substring(5));
        EVA_LOGF("[CommsHub] TIME set %s\n", ok ? "OK" : "FAILED (expected HH:MM)");
    }

    // ---- Date: DATE DD/MM/YYYY ----
    else if (upper.startsWith("DATE ")) {
        String datePart = cmd.substring(5);
        datePart.trim();
        int sl1 = datePart.indexOf('/');
        int sl2 = datePart.lastIndexOf('/');
        if (sl1 > 0 && sl2 > sl1 && (int)datePart.length() == 10) {
            uint8_t  dd   = (uint8_t)datePart.substring(0, sl1).toInt();
            uint8_t  mm   = (uint8_t)datePart.substring(sl1 + 1, sl2).toInt();
            uint16_t yyyy = (uint16_t)datePart.substring(sl2 + 1).toInt();
            bool ok = clock.setManualDate(dd, mm, yyyy);
            EVA_LOGF("[CommsHub] DATE set %s\n", ok ? "OK" : "FAILED (expected DD/MM/YYYY)");
        } else {
            EVA_LOGLN("[CommsHub] DATE parse error - expected DD/MM/YYYY");
        }
    }

    // ---- Alarm ----
    else if (upper == "ALARM OFF") {
        clock.disableAlarm();
    }
    else if (upper.startsWith("ALARM ")) {
        String rest = cmd.substring(6);
        int sp = rest.indexOf(' ');
        if (sp > 0) {
            String hhmm  = rest.substring(0, sp);
            String state = rest.substring(sp + 1);
            state.trim();
            state.toUpperCase();
            if (hhmm.length() == 5 && hhmm.charAt(2) == ':' && state == "ON") {
                uint8_t hh = (uint8_t)hhmm.substring(0, 2).toInt();
                uint8_t mm = (uint8_t)hhmm.substring(3, 5).toInt();
                if (hh <= 23 && mm <= 59) {
                    clock.setAlarm(hh, mm, true);
                }
            }
        }
    }

    // ---- Volume: VOLUME 0-255 / VOL 0-255 / V 0-255 ----
    else if (upper.startsWith("VOLUME ") || upper.startsWith("VOL ") || upper.startsWith("V ")) {
        int sp = cmd.indexOf(' ');
        if (sp > 0) {
            String valStr = cmd.substring(sp + 1);
            valStr.trim();
            int val = valStr.toInt();
            if (val >= 0 && val <= 255) {
                buzzer.setVolume((uint8_t)val);
                EVA_LOGF("[CommsHub] VOLUME set to %d\n", val);
            } else {
                EVA_LOGLN("[CommsHub] VOLUME out of range (0-255)");
            }
        }
    }

    // ---- Brightness: BRIGHT / BRIGHTNESS / LIGHT / T / B 0-255 ----
    else if (upper.startsWith("BRIGHT ") || upper.startsWith("BRIGHTNESS ") ||
             upper.startsWith("LIGHT ")  || upper.startsWith("T ")) {
        int sp = cmd.indexOf(' ');
        if (sp > 0) {
            String valStr = cmd.substring(sp + 1);
            valStr.trim();
            int val = valStr.toInt();
            if (val >= 0 && val <= 255) {
                setBrightness((uint8_t)val);
                EVA_LOGF("[CommsHub] BRIGHT set to %d\n", val);
            } else {
                EVA_LOGLN("[CommsHub] BRIGHT out of range (0-255)");
            }
        }
    }

    // ---- WiFi: WIFI CONNECT  or  WIFI <SSID> <PASSWORD> ----
    else if (upper.startsWith("WIFI ")) {
        String rest      = cmd.substring(5);
        String restUpper = rest;
        restUpper.trim();
        restUpper.toUpperCase();

        if (restUpper == "CONNECT") {
            clock.triggerWifiConnect();
            EVA_LOGLN("[CommsHub] WiFi reconnect triggered");
        } else {
            rest.trim();
            int sp = rest.indexOf(' ');
            if (sp > 0) {
                String ssid = rest.substring(0, sp);
                String pass = rest.substring(sp + 1);
                pass.trim();
                clock.setWifiCredentials(ssid, pass);
                EVA_LOGF("[CommsHub] WiFi credentials saved SSID: %s\n", ssid.c_str());
            } else {
                EVA_LOGLN("[CommsHub] WIFI syntax: 'WIFI <SSID> <PASSWORD>' or 'WIFI CONNECT'");
            }
        }
    }

    else {
        EVA_LOGF("[CommsHub] Unknown command: %s\n", cmd.c_str());
    }
}

void CommsHub::rcSafetyWatchdog() {
    if (modeEngine.getCurrentId() == MODE_RC && movement.isMoving() &&
        (millis() - lastRcCommandTime > 1500)) {
        movement.stop();
    }
}
