#include "ModeClock.h"
#include "Logger.h"

namespace {
    int centeredX(int pixelWidth) {
        int x = (EVA_SCREEN_WIDTH - pixelWidth) / 2;
        return (x < 0) ? 0 : x;
    }

    String oneLineClean(const String &in) {
        String s = in;
        s.replace("\n", " ");
        s.replace("\r", " ");
        while (s.indexOf("  ") >= 0) s.replace("  ", " ");
        s.trim();
        return s;
    }

    // Word-wraps into two lines, breaking at the nearest space at or
    // before maxChars rather than mid-word.
    void splitTwoLines(const String &src, int maxChars, String &line1, String &line2) {
        String s = oneLineClean(src);
        if ((int)s.length() <= maxChars) { line1 = s; line2 = ""; return; }

        int cut = maxChars;
        while (cut > 0 && s.charAt(cut) != ' ') cut--;
        if (cut <= 0) cut = maxChars;

        line1 = s.substring(0, cut); line1.trim();
        line2 = s.substring(cut);    line2.trim();
        if ((int)line2.length() > maxChars) { line2 = line2.substring(0, maxChars); line2.trim(); }
    }
}

ModeClock::ModeClock(Adafruit_SSD1306 &displayRef, ClockService &clockRef,
                      BuzzerManager &buzzerRef, TouchManager &touchRef,
                      MovementEngine &movementRef)
    : display(displayRef), clock(clockRef), buzzer(buzzerRef), touch(touchRef),
      movement(movementRef),
      uiState(UiState::NORMAL), lastSetupInteraction(0), lastInfoSwap(0),
      showingWeather(true),
      alarmRinging(false), alarmSnoozed(false), alarmRingCycle(0),
      alarmRingStartMs(0), alarmLastToneMs(0), alarmNextRingMs(0),
      alarmTapWindowStartMs(0), alarmTapCount(0), lastTapProcessedMs(0) {}

void ModeClock::enter() {
    EVA_LOGLN("[ModeClock] entering - attempting Wi-Fi");
    movement.lock(); // hard-disable motors for the whole time we're in clock mode -
                      // stops the buzzer's own sound being picked up by the mic and
                      // triggering sound-reactive movement from elsewhere in the app

    // clock.begin() blocks for up to a few seconds doing Wi-Fi + NTP + HTTPS.
    // Paint something immediately so the screen doesn't sit blank/looking
    // frozen for that whole stretch.
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 28);
    display.print("Connecting...");
    display.display();

    clock.begin(); // tries Wi-Fi + NTP once; update() keeps retrying in the background
    uiState = UiState::NORMAL;
    pendingAlarm = clock.getAlarm();
    lastInfoSwap = millis();
}

void ModeClock::exit() {
    movement.unlock(); // hand motor control back for other modes
    // No other teardown required - Wi-Fi is left connected for a fast
    // re-entry next time.
}

void ModeClock::handleTouch() {
    TouchEvent ev = touch.getEvent(); // consume-once - see TouchManager.h
    unsigned long now = millis();

    if (ev == TouchEvent::TAP && (now - lastTapProcessedMs) < 220UL) {
        return;
    }
    if (ev == TouchEvent::TAP) {
        lastTapProcessedMs = now;
    }

    if (alarmRinging) {
        if (ev == TouchEvent::TAP) {
            if (alarmTapCount == 0) {
                alarmTapWindowStartMs = now;
                alarmTapCount = 1;
            } else if (now - alarmTapWindowStartMs <= 1800) {
                alarmTapCount++;
            } else {
                alarmTapWindowStartMs = now;
                alarmTapCount = 1;
            }

            if (alarmTapCount == 1) {
                snoozeAlarm();
                return;
            }
            if (alarmTapCount >= 2) {
                stopAlarmRing();
                clock.disableAlarm();
                return;
            }
            return;
        }
    } else if (alarmSnoozed && ev == TouchEvent::TAP) {
        if (alarmTapCount == 0) {
            alarmTapWindowStartMs = now;
            alarmTapCount = 1;
        } else if (now - alarmTapWindowStartMs <= 2000) {
            alarmTapCount++;
        } else {
            alarmTapWindowStartMs = now;
            alarmTapCount = 1;
        }

        if (alarmTapCount == 1) {
            beginAlarmRing();
            alarmTapCount = 0;
            alarmTapWindowStartMs = 0;
            return;
        }
        if (alarmTapCount >= 2) {
            clock.disableAlarm();
            alarmSnoozed = false;
            alarmTapCount = 0;
            alarmTapWindowStartMs = 0;
            return;
        }
    }

    if (uiState != UiState::NORMAL && (now - lastSetupInteraction) > 8000) {
        clock.setAlarm(pendingAlarm.hour24, pendingAlarm.minute, true);
        uiState = UiState::NORMAL;
    }

    if (ev == TouchEvent::PETTING) {
        lastSetupInteraction = now;
        switch (uiState) {
            case UiState::NORMAL:     uiState = UiState::SET_HOUR;   break;
            case UiState::SET_HOUR:   uiState = UiState::SET_MINUTE; break;
            case UiState::SET_MINUTE:
                clock.setAlarm(pendingAlarm.hour24, pendingAlarm.minute, true);
                uiState = UiState::NORMAL;
                buzzer.playClockChime();
                break;
        }
        return;
    }

    if (ev == TouchEvent::TAP) {
        lastSetupInteraction = now;
        if (uiState == UiState::SET_HOUR) {
            pendingAlarm.hour24 = (pendingAlarm.hour24 + 1) % 24;
        } else if (uiState == UiState::SET_MINUTE) {
            pendingAlarm.minute = (pendingAlarm.minute + 5) % 60;
        } else {
            AlarmSetting a = clock.getAlarm();
            if (a.enabled) clock.disableAlarm();
            else clock.setAlarm(a.hour24, a.minute, true);
        }
    }
}

void ModeClock::handleAlarmState() {
    unsigned long now = millis();
    AlarmSetting a = clock.getAlarm();

    if (clock.checkAlarmDue()) {
        beginAlarmRing();
    }

    if (alarmRinging) {
        if ((now - alarmLastToneMs) >= 1200UL) {
            buzzer.playAlarmBeep();
            alarmLastToneMs = now;
        }
        if ((now - alarmRingStartMs) >= 60000UL) {
            if (alarmRingCycle < 2) {
                alarmRingCycle++;
                snoozeAlarm();
            } else {
                stopAlarmRing();
                clock.disableAlarm();
            }
            return;
        }
    }

    if (!alarmRinging && alarmSnoozed && now >= alarmNextRingMs) {
        beginAlarmRing();
    }

    if (!a.enabled && alarmRinging) {
        stopAlarmRing();
    }
}

void ModeClock::beginAlarmRing() {
    if (alarmRinging) return;
    movement.stop();
    alarmRinging = true;
    alarmSnoozed = false;
    alarmRingStartMs = millis();
    alarmLastToneMs = 0;
    buzzer.playAlarmBeep();
    alarmLastToneMs = millis();
}

void ModeClock::snoozeAlarm() {
    if (!alarmRinging && alarmSnoozed) return;
    alarmRinging = false;
    alarmSnoozed = true;
    alarmNextRingMs = millis() + 300000UL;
    alarmLastToneMs = 0;
    buzzer.stop();
}

void ModeClock::stopAlarmRing() {
    alarmRinging = false;
    alarmSnoozed = false;
    alarmNextRingMs = 0;
    alarmRingStartMs = 0;
    alarmLastToneMs = 0;
    alarmTapCount = 0;
    alarmTapWindowStartMs = 0;
    buzzer.stop();
}

void ModeClock::update() {
    touch.update();
    clock.update();
    movement.stop();
    handleTouch();
    handleAlarmState();

    buzzer.update();

    if (millis() - lastInfoSwap > 5000) {
        showingWeather = !showingWeather;
        lastInfoSwap = millis();
    }

    render();
}

void ModeClock::render() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    if (uiState == UiState::NORMAL) drawNormal();
    else drawSetup();

    display.display();
}

void ModeClock::drawNormal() {
    AlarmSetting a = clock.getAlarm();

    // --- Top row: alarm status (left) + Wi-Fi status (right) ---
    display.setTextSize(1);
    display.setCursor(0, 0);
    if (a.enabled) display.printf("ALM %02d:%02d", a.hour24, a.minute);
    else           display.print("ALM OFF");

    display.setCursor(100, 0);
    display.print(clock.isOnline() ? "WiFi" : "off");

    // --- Second row: date (left) + day (right) ---
    display.setCursor(0, 10);
    display.print(clock.getDateString());

    String day = clock.getDayString();
    display.setCursor(EVA_SCREEN_WIDTH - (int)day.length() * 6, 10);
    display.print(day);

    display.drawLine(0, 19, EVA_SCREEN_WIDTH - 1, 19, SSD1306_WHITE);

    // --- Centered, large time, with AM/PM alongside ---
    String fullTime = clock.getTime12h(); // "07:45 PM"
    String hhmm = fullTime;
    String ampm = "";
    int sp = fullTime.indexOf(' ');
    if (sp > 0) { hhmm = fullTime.substring(0, sp); ampm = fullTime.substring(sp + 1); }

    display.setTextSize(2);
    int hhmmWidthPx = hhmm.length() * 12; // smaller, still readable on 128x64
    int x = centeredX(hhmmWidthPx);
    display.setCursor(x, 22);
    display.print(hhmm);

    if (ampm.length() > 0) {
        display.setTextSize(1);
        display.setCursor(x + hhmmWidthPx + 2, 28);
        display.print(ampm);
    }

    display.drawLine(0, 47, EVA_SCREEN_WIDTH - 1, 47, SSD1306_WHITE);

    // --- Bottom info panel: rotates between quote and weather ---
    // NOTE: divider is drawn at y=47, so rows 48-63 (16px) are all we have -
    // exactly two lines of setTextSize(1) text (8px each), back to back,
    // with zero px to spare. Do not push either line below y=56.
    display.setTextSize(1);
    if (showingWeather) {
        String loc = String(WEATHER_LOCATION_NAME);
        if (loc.length() > 21) loc = loc.substring(0, 21); // 128px / 6px per glyph
        display.setCursor(0, 48);
        display.print(loc);

        // Icon reserves the left ~16px of the second line; temperature text
        // starts after it. Swap in drawWeatherIcon(0, 56) once the bitmap
        // is wired up (see note below).
        String summary = clock.getWeatherSummary();
        if (summary.length() > 15) summary = summary.substring(0, 15);
        display.setCursor(18, 56);
        display.print(summary);
        // drawWeatherIcon(0, 56); // TODO: enable once icon bitmap is added
    } else {
        String q = clock.getQuote();
        q.replace("\n", " ");
        q.replace("\r", " ");
        q.trim();
        if (q.length() > 21) q = q.substring(0, 21) + "...";
        display.setCursor(0, 48);
        display.print(q);
    }
}

void ModeClock::drawSetup() {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(uiState == UiState::SET_HOUR ? "SET ALARM - HOUR" : "SET ALARM - MINUTE");
    display.drawRect(0, 12, EVA_SCREEN_WIDTH, 36, SSD1306_WHITE);

    display.setTextSize(3);
    display.setCursor(20, 22);
    display.printf("%02d:%02d", pendingAlarm.hour24, pendingAlarm.minute);

    display.setTextSize(1);
    display.setCursor(uiState == UiState::SET_HOUR ? 24 : 78, 42);
    display.print("^^");

    display.setCursor(0, 54);
    display.print("Tap: +1   Pet: next/save");
}
