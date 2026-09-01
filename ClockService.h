#pragma once
/*
 * ClockService.h
 * ---------------------------------------------------------
 * Provides time-of-day, alarm state, and (when online)
 * weather + a quote-of-the-moment for Clock Mode.
 *
 * Wi-Fi / weather / quote are always OPTIONAL: if the network
 * is unavailable, ClockService silently falls back to
 * offline defaults and the rest of the firmware behaves
 * exactly the same. Nothing about EVA's core behaviour
 * depends on network connectivity.
 *
 * Time itself does NOT depend on being currently online -
 * once NTP has synced, the ESP32's RTC keeps ticking in RAM
 * for as long as the board stays powered, Wi-Fi or not.
 * update() retries the Wi-Fi connection periodically in the
 * background so weather/quote come back automatically once
 * the network is available again.
 * ---------------------------------------------------------
 */

#include <Arduino.h>
#include "Config.h"

struct AlarmSetting {
    bool enabled = false;
    uint8_t hour24 = 7;
    uint8_t minute = 0;
    bool firedToday = false;
};

class ClockService {
public:
    ClockService();

    // Attempts Wi-Fi + NTP sync. Safe to call even with empty
    // credentials - it will simply time out and stay offline.
    void begin();

    // Call every loop() - handles reconnect + periodic
    // weather/quote refresh without blocking.
    void update();

    bool isOnline() const;

    // ---- Time ----
    // Deliberately NOT gated on Wi-Fi/online state: as long as the
    // ESP32 has synced at least once and stayed powered, this keeps
    // working even if Wi-Fi later drops.
    bool getLocalTime(struct tm &outTimeInfo) const;
    String getTime12h() const;      // "07:45 PM"
    String getDateString() const;   // "01/01/2026"
    String getDayString() const;    // "Saturday"

    // Manual time set (e.g. from the Bluetooth command channel).
    // Accepts 24h "HH:MM".
    bool setManualTimeHHMM(const String &hhmm);
    bool setManualTime(uint8_t hour24, uint8_t minute, uint8_t second = 0);

    // ---- Alarm ----
    void setAlarm(uint8_t hour24, uint8_t minute, bool enabled = true);
    void disableAlarm();
    AlarmSetting getAlarm() const;
    // Returns true exactly once per day at the alarm moment.
    bool checkAlarmDue();

    // ---- Online extras ----
    String getWeatherSummary() const; // e.g. "24C" or "Offline"
    String getQuote() const;          // falls back to a local default set

private:
    bool online;
    bool wifiTriedOnce;
    bool wifiOfflineLogged;
    bool wifiOnlineLogged;
    unsigned long lastWifiRetry;
    unsigned long lastWeatherFetch;
    unsigned long lastQuoteFetch;

    String weatherSummary;
    String quoteText;

    AlarmSetting alarm;

    void tryConnectWifi();
    void fetchWeather();
    void fetchQuote();
    String pickOfflineQuote() const;
};
