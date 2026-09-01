#include "ModeClock.h"
#include "CommsHub.h"   // needed here (not in .h) to call pComms->isConnected()
#include "Logger.h"

// ============================================================
// Pixel layout constants for the 128x64 OLED
// ============================================================
namespace {
    // ── Top header (two rows) ───────────────────────────────
    // Row 0: Bell icon + ALM status    | WiFi icon | BT icon
    static const int HDR_ROW1_Y   = 0;   // y for row 0 (alarm + icons)
    static const int HDR_ROW2_Y   = 9;   // y for row 1 (calendar + date + day)

    // Icons — top-right corner (8 px each, 1 px gap)
    static const int WIFI_ICON_X  = 109; // WiFi  8x8
    static const int BT_ICON_X    = 119; // BT    8x8  (ends at 127)

    // ── Time block ──────────────────────────────────────────
    // No divider above the time (removed per user request).
    // textSize(3) → each char cell = 18 px wide, 24 px tall.
    // "HH:MM" = 5 chars × 18 = 90 px; centred in 128 px → x=19.
    static const int TIME_Y       = 19;  // top of time digit row
    static const int TIME_X       = 19;  // left edge of "HH:MM" block
    static const int TIME_W       = 90;  // total width of "HH:MM"
    static const int AMPM_X       = TIME_X + TIME_W + 3; // 19+90+3=112
    static const int AMPM_Y       = TIME_Y + 9;          // vertically centred

    // ── Bottom panel ────────────────────────────────────────
    // (Keep EXACTLY as approved — user said this is perfect)
    static const int DIVIDER_BOT  = 45;
    static const int LOC_ROW      = 47;
    static const int TEMP_ROW     = 56;
    static const int WEATHER_ICON_X  = 68;
    static const int WEATHER_ICON_Y  = 48;
    static const int WEATHER_TEXT_X  = 80;
    static const int WEATHER_TEXT_Y  = 48;
}

// ============================================================
// Constructor
// ============================================================
ModeClock::ModeClock(Adafruit_SSD1306 &displayRef, ClockService &clockRef,
                      BuzzerManager &buzzerRef, TouchManager &touchRef,
                      MovementEngine &movementRef)
    : display(displayRef), clock(clockRef), buzzer(buzzerRef), touch(touchRef),
      movement(movementRef),
      uiState(UiState::NORMAL), lastSetupInteraction(0),
      alarmRinging(false), alarmSnoozed(false), alarmRingCycle(0),
      alarmRingStartMs(0), alarmLastToneMs(0), alarmNextRingMs(0),
      alarmTapWindowStartMs(0), alarmTapCount(0), lastTapProcessedMs(0) {}

// ============================================================
// Mode lifecycle
// ============================================================
void ModeClock::enter() {
    EVA_LOGLN("[ModeClock] entering");
    movement.lock();

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 28);
    display.print("Connecting...");
    display.display();

    clock.begin();
    uiState      = UiState::NORMAL;
    pendingAlarm = clock.getAlarm();
}

void ModeClock::exit() {
    movement.unlock();
}

// ============================================================
// Touch handling
// ============================================================
void ModeClock::handleTouch() {
    TouchEvent ev = touch.getEvent();
    unsigned long now = millis();

    if (ev == TouchEvent::TAP && (now - lastTapProcessedMs) < 220UL) return;
    if (ev == TouchEvent::TAP) lastTapProcessedMs = now;

    if (alarmRinging) {
        if (ev == TouchEvent::TAP) {
            if (alarmTapCount == 0) { alarmTapWindowStartMs = now; alarmTapCount = 1; }
            else if (now - alarmTapWindowStartMs <= 1800) { alarmTapCount++; }
            else { alarmTapWindowStartMs = now; alarmTapCount = 1; }
            if (alarmTapCount == 1)  { snoozeAlarm(); return; }
            if (alarmTapCount >= 2)  { stopAlarmRing(); clock.disableAlarm(); return; }
            return;
        }
    } else if (alarmSnoozed && ev == TouchEvent::TAP) {
        if (alarmTapCount == 0) { alarmTapWindowStartMs = now; alarmTapCount = 1; }
        else if (now - alarmTapWindowStartMs <= 2000) { alarmTapCount++; }
        else { alarmTapWindowStartMs = now; alarmTapCount = 1; }
        if (alarmTapCount == 1)  { beginAlarmRing(); alarmTapCount = 0; alarmTapWindowStartMs = 0; return; }
        if (alarmTapCount >= 2)  { clock.disableAlarm(); alarmSnoozed = false; alarmTapCount = 0; alarmTapWindowStartMs = 0; return; }
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
        if      (uiState == UiState::SET_HOUR)   pendingAlarm.hour24 = (pendingAlarm.hour24 + 1) % 24;
        else if (uiState == UiState::SET_MINUTE) pendingAlarm.minute = (pendingAlarm.minute + 5) % 60;
        else {
            AlarmSetting a = clock.getAlarm();
            if (a.enabled) clock.disableAlarm();
            else clock.setAlarm(a.hour24, a.minute, true);
        }
    }
}

// ============================================================
// Alarm state machine
// ============================================================
void ModeClock::handleAlarmState() {
    unsigned long now = millis();
    AlarmSetting a = clock.getAlarm();

    if (clock.checkAlarmDue()) beginAlarmRing();

    if (alarmRinging) {
        if ((now - alarmLastToneMs) >= 1200UL) { buzzer.playAlarmBeep(); alarmLastToneMs = now; }
        if ((now - alarmRingStartMs) >= 60000UL) {
            if (alarmRingCycle < 2) { alarmRingCycle++; snoozeAlarm(); }
            else { stopAlarmRing(); clock.disableAlarm(); }
            return;
        }
    }

    if (!alarmRinging && alarmSnoozed && now >= alarmNextRingMs) beginAlarmRing();
    if (!a.enabled && alarmRinging) stopAlarmRing();
}

void ModeClock::beginAlarmRing() {
    if (alarmRinging) return;
    movement.stop();
    alarmRinging     = true;
    alarmSnoozed     = false;
    alarmRingStartMs = millis();
    alarmLastToneMs  = 0;
    buzzer.playAlarmBeep();
    alarmLastToneMs  = millis();
}

void ModeClock::snoozeAlarm() {
    if (!alarmRinging && alarmSnoozed) return;
    alarmRinging    = false;
    alarmSnoozed    = true;
    alarmNextRingMs = millis() + 300000UL;
    alarmLastToneMs = 0;
    buzzer.stop();
}

void ModeClock::stopAlarmRing() {
    alarmRinging = alarmSnoozed = false;
    alarmNextRingMs = alarmRingStartMs = alarmLastToneMs = 0;
    alarmTapCount   = 0;
    alarmTapWindowStartMs = 0;
    buzzer.stop();
}

// ============================================================
// Main update loop
// ============================================================
void ModeClock::update() {
    touch.update();
    clock.update();
    movement.stop();
    handleTouch();
    handleAlarmState();
    buzzer.update();
    render();
}

void ModeClock::render() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    if (uiState == UiState::NORMAL) drawNormal();
    else                            drawSetup();
    display.display();
}

// ============================================================
// Weather icon selector
// ============================================================
const unsigned char* ModeClock::weatherConditionIcon(const String &condition) {
    if (condition.startsWith("Sunny") || condition.startsWith("Mostly"))
        return icon_sun;
    if (condition.startsWith("Rain")  || condition.startsWith("Drizzle") ||
        condition.startsWith("Shower")|| condition.startsWith("Frzg"))
        return icon_rain;
    if (condition.startsWith("Snowy") || condition.startsWith("Snw"))
        return icon_snow;
    if (condition.startsWith("Thunder"))
        return icon_thunder;
    return icon_cloud; // Cloudy, Foggy, PrtlCloud, Offline, Unknown
}

// ============================================================
// Normal clock face — matches user photo & reference image
// ============================================================
void ModeClock::drawNormal() {
    AlarmSetting a       = clock.getAlarm();
    bool         online  = clock.isOnline();
    bool         btConn  = pComms ? pComms->isConnected() : false;

    // ── ROW 0 (y=0): Bell + ALM status | WiFi | BT ──────────
    display.drawBitmap(0, HDR_ROW1_Y, icon_alarm, 8, 8, SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(10, HDR_ROW1_Y + 1); // +1 vertically centres 7px text in 8px row
    display.print(a.enabled ? "ALM ON" : "ALM OFF");

    // WiFi: normal icon if online, X if not
    display.drawBitmap(WIFI_ICON_X, HDR_ROW1_Y,
                       online ? icon_wifi : icon_no_signal, 8, 8, SSD1306_WHITE);

    // BT: normal icon if connected, X if not
    display.drawBitmap(BT_ICON_X, HDR_ROW1_Y,
                       btConn ? icon_bluetooth : icon_no_signal, 8, 8, SSD1306_WHITE);

    // ── ROW 1 (y=9): Calendar + Date | 3-letter Day ──────────
    display.drawBitmap(0, HDR_ROW2_Y, icon_calendar, 8, 8, SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(10, HDR_ROW2_Y + 1);
    display.print(clock.getDateString()); // "14/08/2026"

    // 3-letter day abbreviation, right-aligned (max 18 px = 3 chars × 6 px)
    String day = clock.getDayString();    // "Saturday"
    day = day.substring(0, min(3, (int)day.length()));
    day.toUpperCase();                    // "SAT"
    display.setCursor(EVA_SCREEN_WIDTH - (int)day.length() * 6, HDR_ROW2_Y + 1);
    display.print(day);

    // ── NO top divider line (removed per user request) ───────

    // ── TIME BLOCK (y=19): centred HH:MM at textSize(3) ─────
    String fullTime = clock.getTime12h(); // "04:51 PM"
    String hhmm = fullTime;
    String ampm = "";
    int sp = fullTime.indexOf(' ');
    if (sp > 0) { hhmm = fullTime.substring(0, sp); ampm = fullTime.substring(sp + 1); }

    display.setTextSize(3);
    display.setCursor(TIME_X, TIME_Y);
    display.print(hhmm); // "04:51"  — textSize(3) = 18 px/char, 24 px tall

    // AM/PM in textSize(1), vertically centred beside the digits
    if (ampm.length() > 0) {
        display.setTextSize(1);
        display.setCursor(AMPM_X, AMPM_Y);
        display.print(ampm);
    }

    // ── DIVIDER (y=45) ────────────────────────────────────────
    display.drawLine(0, DIVIDER_BOT, EVA_SCREEN_WIDTH - 1, DIVIDER_BOT, SSD1306_WHITE);

    // ── BOTTOM PANEL — KEEP EXACTLY AS APPROVED ───────────────
    display.setTextSize(1);

    String loc = String(WEATHER_LOCATION_NAME);
    if (loc.length() > 10) loc = loc.substring(0, 10);
    display.setCursor(0, LOC_ROW);
    display.print(loc);

    String temp = clock.getWeatherSummary(); // "26.1°C"
    if (temp.length() > 10) temp = temp.substring(0, 10);
    display.setCursor(0, TEMP_ROW);
    display.print(temp);

    String cond = clock.getWeatherCondition();
    const unsigned char *wicon = weatherConditionIcon(cond);
    display.drawBitmap(WEATHER_ICON_X, WEATHER_ICON_Y, wicon, 8, 8, SSD1306_WHITE);

    if (cond.length() > 7) cond = cond.substring(0, 7);
    display.setCursor(WEATHER_TEXT_X, WEATHER_TEXT_Y + 1);
    display.print(cond);
}

// ============================================================
// Alarm setup screen
// ============================================================
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
