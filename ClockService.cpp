#include "ClockService.h"
#include "Logger.h"

#include <WiFi.h>
#include <time.h>
#include <sys/time.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

namespace {
    const unsigned long WIFI_RETRY_MS = 15000UL;   // retry Wi-Fi every 15s while offline
    const unsigned long FAST_RETRY_MS = 15000UL;   // retry weather/quote quickly while stale
    const int HTTP_TIMEOUT_MS = 9000;
    const unsigned long NTP_WAIT_MS = 6000UL;      // wait up to 6s for the first sync
}

ClockService::ClockService()
    : online(false),
      wifiTriedOnce(false),
      wifiOfflineLogged(false),
      wifiOnlineLogged(false),
      lastWifiRetry(0),
      lastWeatherFetch(0),
      lastQuoteFetch(0),
      weatherSummary("Offline"),
      quoteText("") {}

void ClockService::tryConnectWifi() {
    if (strlen(WIFI_SSID) == 0) {
        online = false;
        if (!wifiOfflineLogged) {
            EVA_LOGLN("[ClockService] No Wi-Fi credentials configured - staying offline");
            wifiOfflineLogged = true;
            wifiOnlineLogged = false;
        }
        return;
    }

    WiFi.mode(WIFI_STA);
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT_MS) {
        delay(200); // one-time connection wait, not a runtime-loop delay
    }

    online = (WiFi.status() == WL_CONNECTED);
    wifiTriedOnce = true;
    lastWifiRetry = millis();

    if (!online) {
        if (!wifiOfflineLogged) {
            EVA_LOGLN("[ClockService] Wi-Fi connection failed - offline mode");
            wifiOfflineLogged = true;
            wifiOnlineLogged = false;
        }
        return;
    }

    if (!wifiOnlineLogged) {
        EVA_LOGF("[ClockService] Wi-Fi connected, IP=%s RSSI=%d\n",
                 WiFi.localIP().toString().c_str(), WiFi.RSSI());
        wifiOnlineLogged = true;
        wifiOfflineLogged = false;
    }

    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER_1, NTP_SERVER_2);

    struct tm t;
    bool synced = false;
    unsigned long ntpStart = millis();
    while ((millis() - ntpStart) < NTP_WAIT_MS) {
        if (::getLocalTime(&t, 200)) { synced = true; break; }
        delay(100);
    }
    EVA_LOGLN(synced ? "[ClockService] NTP sync OK" : "[ClockService] NTP sync pending");
}

void ClockService::begin() {
    tryConnectWifi();
    if (quoteText.length() == 0) {
        quoteText = pickOfflineQuote();
    }
    if (online) {
        fetchWeather();
        fetchQuote();
    } else {
        weatherSummary = "Offline";
    }
}

bool ClockService::isOnline() const {
    return online;
}

bool ClockService::getLocalTime(struct tm &outTimeInfo) const {
    // Important: NOT gated on the online flag. The ESP32 keeps RTC
    // time in RAM for as long as it stays powered, independent of
    // whether Wi-Fi is currently connected.
    return ::getLocalTime(&outTimeInfo, 200);
}

String ClockService::getTime12h() const {
    struct tm t;
    if (!getLocalTime(t)) return "--:-- --";
    char buf[16];
    strftime(buf, sizeof(buf), "%I:%M %p", &t);
    return String(buf);
}

String ClockService::getDateString() const {
    struct tm t;
    if (!getLocalTime(t)) return "--/--/----";
    char buf[16];
    strftime(buf, sizeof(buf), "%d/%m/%Y", &t);
    return String(buf);
}

String ClockService::getDayString() const {
    struct tm t;
    if (!getLocalTime(t)) return "---";
    char buf[16];
    strftime(buf, sizeof(buf), "%A", &t);
    return String(buf);
}

bool ClockService::setManualTime(uint8_t hour24, uint8_t minute, uint8_t second) {
    if (hour24 > 23 || minute > 59 || second > 59) return false;

    struct tm t;
    if (!::getLocalTime(&t, 200)) {
        // Never synced yet - seed a sane placeholder date so mktime()
        // still produces something usable.
        t = {};
        t.tm_year = 126; // 2026
        t.tm_mon = 0;
        t.tm_mday = 1;
    }
    t.tm_hour = hour24;
    t.tm_min = minute;
    t.tm_sec = second;

    time_t epoch = mktime(&t);
    if (epoch < 0) return false;

    struct timeval tv;
    tv.tv_sec = epoch;
    tv.tv_usec = 0;
    if (settimeofday(&tv, nullptr) != 0) return false;

    EVA_LOGF("[ClockService] Manual time set to %02u:%02u:%02u\n", hour24, minute, second);
    return true;
}

bool ClockService::setManualTimeHHMM(const String &hhmm) {
    if (hhmm.length() != 5 || hhmm.charAt(2) != ':') return false;

    char h1 = hhmm.charAt(0), h2 = hhmm.charAt(1);
    char m1 = hhmm.charAt(3), m2 = hhmm.charAt(4);
    if (!isDigit(h1) || !isDigit(h2) || !isDigit(m1) || !isDigit(m2)) return false;

    uint8_t hh = (uint8_t)((h1 - '0') * 10 + (h2 - '0'));
    uint8_t mm = (uint8_t)((m1 - '0') * 10 + (m2 - '0'));
    return setManualTime(hh, mm, 0);
}

void ClockService::setAlarm(uint8_t hour24, uint8_t minute, bool enabled) {
    alarm.hour24 = hour24;
    alarm.minute = minute;
    alarm.enabled = enabled;
    alarm.firedToday = false;
}

void ClockService::disableAlarm() {
    alarm.enabled = false;
}

AlarmSetting ClockService::getAlarm() const {
    return alarm;
}

bool ClockService::checkAlarmDue() {
    if (!alarm.enabled) return false;

    struct tm t;
    if (!getLocalTime(t)) return false;

    if (t.tm_hour == alarm.hour24 && t.tm_min == alarm.minute) {
        if (!alarm.firedToday) {
            alarm.firedToday = true;
            return true;
        }
    } else {
        alarm.firedToday = false;
    }
    return false;
}

void ClockService::fetchWeather() {
    if (!online) return;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = "https://api.open-meteo.com/v1/forecast?latitude=" WEATHER_LATITUDE
                 "&longitude=" WEATHER_LONGITUDE
                 "&current=temperature_2m&timezone=auto";
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    http.setTimeout(HTTP_TIMEOUT_MS);

    if (!http.begin(client, url)) {
        EVA_LOGLN("[ClockService] Weather begin() failed");
        lastWeatherFetch = millis();
        return;
    }

    int code = http.GET();
    if (code == 200) {
        String payload = http.getString();
        StaticJsonDocument<768> doc;
        if (!deserializeJson(doc, payload)) {
            if (doc.containsKey("current")) {
                float tempC = doc["current"]["temperature_2m"] | NAN;
                if (!isnan(tempC)) {
                    weatherSummary = String(tempC, 1) + "C";
                } else {
                    weatherSummary = "Offline";
                }
            } else {
                weatherSummary = "Offline";
            }
        } else {
            weatherSummary = "Offline";
        }
    } else {
        EVA_LOGF("[ClockService] Weather fetch failed, HTTP %d\n", code);
        weatherSummary = "Offline";
    }
    http.end();
    lastWeatherFetch = millis();
}

void ClockService::fetchQuote() {
    if (!online) return;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    http.setTimeout(HTTP_TIMEOUT_MS);

    if (!http.begin(client, "https://zenquotes.io/api/random")) {
        EVA_LOGLN("[ClockService] Quote begin() failed");
        lastQuoteFetch = millis();
        return;
    }

    int code = http.GET();
    if (code == 200) {
        String payload = http.getString();
        StaticJsonDocument<1024> doc;
        if (!deserializeJson(doc, payload)) {
            const char *q = doc[0]["q"] | "";
            const char *a = doc[0]["a"] | "";
            if (strlen(q) > 0) {
                String built = String(q);
                if (strlen(a) > 0) {
                    built += " - ";
                    built += String(a);
                }
                built.trim();
                if (built.length() > 100) built = built.substring(0, 97) + "...";
                quoteText = built;
            }
        }
    } else {
        EVA_LOGF("[ClockService] Quote fetch failed, HTTP %d\n", code);
    }
    http.end();
    lastQuoteFetch = millis();
}

String ClockService::pickOfflineQuote() const {
    static const char *offlineQuotes[] = {
        "Small steps move you onward.",
        "A quiet day is still a good day.",
        "Curiosity grows the mind.",
        "Rest is part of growth."
    };
    uint8_t idx = random(0, 4);
    return String(offlineQuotes[idx]);
}

void ClockService::update() {
    unsigned long now = millis();

    bool wasOnline = online;
    online = (WiFi.status() == WL_CONNECTED);

    if (!online) {
        weatherSummary = "Offline";
        if (!wifiTriedOnce || (now - lastWifiRetry) >= WIFI_RETRY_MS) {
            tryConnectWifi();
        }
        if (!online) return;
    }

    if (!wasOnline && online) {
        // Just came back online - fetch soon rather than waiting a
        // full refresh interval.
        lastWeatherFetch = 0;
        lastQuoteFetch = 0;
    }

    unsigned long weatherInterval = (weatherSummary == "Offline") ? FAST_RETRY_MS : WEATHER_REFRESH_INTERVAL_MS;
    unsigned long quoteInterval = (quoteText.length() == 0) ? FAST_RETRY_MS : QUOTE_REFRESH_INTERVAL_MS;

    if (now - lastWeatherFetch >= weatherInterval) fetchWeather();
    if (now - lastQuoteFetch >= quoteInterval) fetchQuote();

    if (quoteText.length() == 0) quoteText = pickOfflineQuote();
}

String ClockService::getWeatherSummary() const {
    return weatherSummary.length() ? weatherSummary : "Offline";
}

String ClockService::getQuote() const {
    return quoteText.length() ? quoteText : pickOfflineQuote();
}
