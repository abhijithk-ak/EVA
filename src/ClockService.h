#pragma once
/*
 * ClockService.h
 * ---------------------------------------------------------
 * Provides time-of-day, alarm state, and (when online)
 * weather data for Clock Mode.
 *
 * Wi-Fi / weather are OPTIONAL. If the network is unavailable,
 * ClockService silently falls back to offline defaults.
 *
 * WiFi retry phases (saves power — BLE stays on always):
 *   INITIAL: retry every 60 s for the first 10 minutes.
 *   IDLE:    retry every 40-60 min (random) once INITIAL exhausted.
 *   Entering Clock Mode always resets to INITIAL.
 *   "WIFI CONNECT" BLE command also resets to INITIAL.
 *
 * Weather: fetched once on mode entry (if online), then every 1 hr.
 * Time:    NTP synced on first connect; ESP32 RTC keeps it afterwards.
 *
 * WiFi credentials can be updated at runtime via setWifiCredentials()
 * (also persisted to NVS via Preferences so they survive reboots).
 * Falls back to WIFI_SSID / WIFI_PASSWORD in Config.h if none saved.
 * ---------------------------------------------------------
 */

#include <Arduino.h>
#include <Preferences.h>
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

    // Loads saved credentials, resets WiFi phase to INITIAL, and
    // attempts a first connection + NTP sync. Safe with empty credentials.
    void begin();

    // Call every loop() — phase-based WiFi retry + hourly weather refresh.
    void update();

    bool isOnline() const;

    // ---- Time ----
    bool getLocalTime(struct tm &outTimeInfo) const;
    String getTime12h() const;      // "07:45 PM"
    String getDateString() const;   // "01/01/2026"
    String getDayString() const;    // "Saturday"

    // Manual setters (BLE commands TIME / DATE).
    bool setManualTimeHHMM(const String &hhmm);
    bool setManualTime(uint8_t hour24, uint8_t minute, uint8_t second = 0);
    bool setManualDate(uint8_t day, uint8_t month, uint16_t year);

    // ---- Alarm ----
    void setAlarm(uint8_t hour24, uint8_t minute, bool enabled = true);
    void disableAlarm();
    AlarmSetting getAlarm() const;
    bool checkAlarmDue();

    // ---- Weather ----
    String getWeatherSummary() const;    // e.g. "26.1°C" or "Offline"
    String getWeatherCondition() const;  // e.g. "Cloudy", "Rainy", "Sunny"

    // ---- WiFi credential management ----
    // Saves SSID + password to NVS (persists across reboots) and uses
    // them for all subsequent connection attempts in this session.
    void setWifiCredentials(const String &ssid, const String &password);

    // Resets retry phase to INITIAL so the next update() call triggers
    // an immediate reconnect attempt. Use after WIFI CONNECT BLE command.
    void triggerWifiConnect();

private:
    enum class WifiPhase { INITIAL, IDLE };

    bool online;
    bool wifiOfflineLogged;
    bool wifiOnlineLogged;

    // Phase-based retry tracking
    WifiPhase     wifiPhase;
    unsigned long lastWifiRetry;       // millis() of last tryConnectWifi() call
    uint8_t       initialRetriesDone;  // attempts made in INITIAL phase (max 10)
    unsigned long idleRetryIntervalMs; // randomly chosen 40-60 min IDLE interval

    // Weather
    unsigned long lastWeatherFetch;
    unsigned long lastWeatherAttempt;
    String weatherSummary;
    String weatherCondition;

    // Runtime WiFi credentials (override Config.h defines when non-empty)
    String runtimeSsid;
    String runtimePassword;

    AlarmSetting alarm;

    void tryConnectWifi();
    void fetchWeather();
    void loadSavedCredentials();
    static String weatherCodeToCondition(int code);
};
