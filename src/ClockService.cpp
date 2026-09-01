#include "ClockService.h"
#include "Logger.h"

#include <WiFi.h>
#include <time.h>
#include <sys/time.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

namespace {
    const int           HTTP_TIMEOUT_MS  = 9000;
    const unsigned long NTP_WAIT_MS      = 6000UL;
    // WiFi initial phase: retry every 60 s, up to INITIAL_MAX_RETRIES attempts.
    const unsigned long INITIAL_RETRY_MS = 60000UL;  // 1 min
    const uint8_t       INITIAL_MAX_RETRIES = 10;    // covers first ~10 min
    // IDLE phase: random interval between 40 and 60 minutes.
    const unsigned long IDLE_MIN_MS  = 2400000UL; // 40 min
    const unsigned long IDLE_RANGE_MS = 1200000UL; // + 0-20 min extra
}

ClockService::ClockService()
    : online(false),
      wifiOfflineLogged(false),
      wifiOnlineLogged(false),
      wifiPhase(WifiPhase::INITIAL),
      lastWifiRetry(0),
      initialRetriesDone(0),
      idleRetryIntervalMs(IDLE_MIN_MS),
      lastWeatherFetch(0),
      lastWeatherAttempt(0),
      weatherSummary("Offline"),
      weatherCondition("Offline") {}

// ---------------------------------------------------------------------------
// Credentials
// ---------------------------------------------------------------------------
void ClockService::loadSavedCredentials() {
    Preferences prefs;
    if (prefs.begin("eva_wifi", /*readOnly=*/true)) {
        String s = prefs.getString("ssid", "");
        String p = prefs.getString("pass", "");
        prefs.end();
        if (s.length() > 0) {
            runtimeSsid     = s;
            runtimePassword = p;
            EVA_LOGF("[ClockService] Loaded saved WiFi SSID: %s\n", s.c_str());
        }
    }
}

void ClockService::setWifiCredentials(const String &ssid, const String &password) {
    runtimeSsid     = ssid;
    runtimePassword = password;
    Preferences prefs;
    if (prefs.begin("eva_wifi", /*readOnly=*/false)) {
        prefs.putString("ssid", ssid);
        prefs.putString("pass", password);
        prefs.end();
    }
    EVA_LOGF("[ClockService] WiFi credentials updated SSID: %s\n", ssid.c_str());
}

void ClockService::triggerWifiConnect() {
    wifiPhase         = WifiPhase::INITIAL;
    initialRetriesDone = 0;
    lastWifiRetry     = 0; // forces immediate retry on next update()
    EVA_LOGLN("[ClockService] WiFi connect triggered — resetting to INITIAL phase");
}

// ---------------------------------------------------------------------------
// WiFi connection
// ---------------------------------------------------------------------------
void ClockService::tryConnectWifi() {
    // Determine which credentials to use: runtime (BLE-set) > Config.h
    const char *ssid = (runtimeSsid.length() > 0) ? runtimeSsid.c_str()
                                                   : WIFI_SSID;
    const char *pass = (runtimeSsid.length() > 0) ? runtimePassword.c_str()
                                                   : WIFI_PASSWORD;

    if (strlen(ssid) == 0) {
        online = false;
        if (!wifiOfflineLogged) {
            EVA_LOGLN("[ClockService] No WiFi credentials configured - staying offline");
            wifiOfflineLogged = true;
            wifiOnlineLogged  = false;
        }
        lastWifiRetry = millis();
        return;
    }

    WiFi.mode(WIFI_STA);
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.begin(ssid, pass);
    }

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT_MS) {
        delay(200);
    }

    online = (WiFi.status() == WL_CONNECTED);
    lastWifiRetry = millis();

    if (!online) {
        if (!wifiOfflineLogged) {
            EVA_LOGF("[ClockService] WiFi connection failed (SSID: %s)\n", ssid);
            wifiOfflineLogged = true;
            wifiOnlineLogged  = false;
        }
        return;
    }

    if (!wifiOnlineLogged) {
        EVA_LOGF("[ClockService] WiFi connected IP=%s RSSI=%d\n",
                 WiFi.localIP().toString().c_str(), WiFi.RSSI());
        wifiOnlineLogged  = true;
        wifiOfflineLogged = false;
    }

    // NTP sync — only on first successful connect each session.
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

// ---------------------------------------------------------------------------
// begin() — called when entering Clock Mode
// ---------------------------------------------------------------------------
void ClockService::begin() {
    loadSavedCredentials();

    // Reset to INITIAL phase so mode-switch always triggers fresh attempts.
    wifiPhase          = WifiPhase::INITIAL;
    initialRetriesDone = 0;
    lastWifiRetry      = 0;

    tryConnectWifi(); // first attempt immediately

    if (online) {
        lastWeatherFetch   = 0;
        lastWeatherAttempt = 0;
        fetchWeather();
    } else {
        weatherSummary   = "Offline";
        weatherCondition = "Offline";
    }
}

// ---------------------------------------------------------------------------
// update() — call every loop() while in Clock Mode
// ---------------------------------------------------------------------------
void ClockService::update() {
    unsigned long now = millis();

    bool wasOnline = online;
    online = (WiFi.status() == WL_CONNECTED);

    // ---- Handle offline / retry scheduling ----
    if (!online) {
        weatherSummary   = "Offline";
        weatherCondition = "Offline";

        bool shouldRetry = false;

        if (wifiPhase == WifiPhase::INITIAL) {
            // Retry every 60 s for the first 10 minutes
            if ((now - lastWifiRetry) >= INITIAL_RETRY_MS) {
                shouldRetry = true;
            }
        } else {
            // IDLE phase: retry once every 40-60 min
            if ((now - lastWifiRetry) >= idleRetryIntervalMs) {
                shouldRetry = true;
            }
        }

        if (shouldRetry) {
            tryConnectWifi();
            online = (WiFi.status() == WL_CONNECTED);

            if (!online) {
                if (wifiPhase == WifiPhase::INITIAL) {
                    initialRetriesDone++;
                    if (initialRetriesDone >= INITIAL_MAX_RETRIES) {
                        // Initial phase exhausted — switch to low-power IDLE retry
                        wifiPhase = WifiPhase::IDLE;
                        idleRetryIntervalMs = IDLE_MIN_MS + random(0, IDLE_RANGE_MS);
                        EVA_LOGF("[ClockService] WiFi IDLE phase, next retry in %lu min\n",
                                 idleRetryIntervalMs / 60000UL);
                    }
                } else {
                    // Pick a new random idle interval after each failed IDLE attempt
                    idleRetryIntervalMs = IDLE_MIN_MS + random(0, IDLE_RANGE_MS);
                }
            }
        }

        if (!online) return;
    }

    // ---- Just came back online ----
    if (!wasOnline && online) {
        lastWeatherFetch   = 0;
        lastWeatherAttempt = 0;
    }

    // ---- Hourly weather refresh ----
    if ((now - lastWeatherFetch) >= WEATHER_REFRESH_INTERVAL_MS) {
        fetchWeather();
    }
}

// ---------------------------------------------------------------------------
// Time helpers
// ---------------------------------------------------------------------------
bool ClockService::isOnline() const { return online; }

bool ClockService::getLocalTime(struct tm &outTimeInfo) const {
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
        t = {}; t.tm_year = 126; t.tm_mon = 0; t.tm_mday = 1;
    }
    t.tm_hour = hour24; t.tm_min = minute; t.tm_sec = second;
    t.tm_isdst = -1;
    time_t epoch = mktime(&t);
    if (epoch < 0) return false;
    struct timeval tv; tv.tv_sec = epoch; tv.tv_usec = 0;
    if (settimeofday(&tv, nullptr) != 0) return false;
    EVA_LOGF("[ClockService] Time set to %02u:%02u:%02u\n", hour24, minute, second);
    return true;
}

bool ClockService::setManualTimeHHMM(const String &hhmm) {
    if (hhmm.length() != 5 || hhmm.charAt(2) != ':') return false;
    char h1=hhmm.charAt(0), h2=hhmm.charAt(1), m1=hhmm.charAt(3), m2=hhmm.charAt(4);
    if (!isDigit(h1)||!isDigit(h2)||!isDigit(m1)||!isDigit(m2)) return false;
    return setManualTime((h1-'0')*10+(h2-'0'), (m1-'0')*10+(m2-'0'), 0);
}

bool ClockService::setManualDate(uint8_t day, uint8_t month, uint16_t year) {
    if (day<1||day>31||month<1||month>12||year<2000||year>2099) return false;
    struct tm t;
    if (!::getLocalTime(&t, 200)) { t = {}; t.tm_hour=0; t.tm_min=0; t.tm_sec=0; }
    t.tm_year = (int)year-1900; t.tm_mon = (int)month-1; t.tm_mday = (int)day;
    t.tm_isdst = -1;
    time_t epoch = mktime(&t);
    if (epoch < 0) return false;
    struct timeval tv; tv.tv_sec = epoch; tv.tv_usec = 0;
    if (settimeofday(&tv, nullptr) != 0) return false;
    EVA_LOGF("[ClockService] Date set to %02u/%02u/%04u\n", day, month, year);
    return true;
}

// ---------------------------------------------------------------------------
// Alarm
// ---------------------------------------------------------------------------
void ClockService::setAlarm(uint8_t hour24, uint8_t minute, bool enabled) {
    alarm.hour24 = hour24; alarm.minute = minute;
    alarm.enabled = enabled; alarm.firedToday = false;
}
void ClockService::disableAlarm() { alarm.enabled = false; }
AlarmSetting ClockService::getAlarm() const { return alarm; }

bool ClockService::checkAlarmDue() {
    if (!alarm.enabled) return false;
    struct tm t;
    if (!getLocalTime(t)) return false;
    if (t.tm_hour == alarm.hour24 && t.tm_min == alarm.minute) {
        if (!alarm.firedToday) { alarm.firedToday = true; return true; }
    } else { alarm.firedToday = false; }
    return false;
}

// ---------------------------------------------------------------------------
// WMO code → condition string
// Reference: https://open-meteo.com/en/docs (weathervariables section)
// ---------------------------------------------------------------------------
String ClockService::weatherCodeToCondition(int code) {
    if (code == 0)                  return "Sunny";
    if (code == 1)                  return "MostlyClear";  // fits 7-char display cap
    if (code == 2)                  return "PrtlCloud";
    if (code == 3)                  return "Cloudy";
    if (code == 45 || code == 48)   return "Foggy";
    if (code >= 51 && code <= 55)   return "Drizzle";
    if (code >= 56 && code <= 57)   return "IcyDrzzl";
    if (code >= 61 && code <= 65)   return "Rainy";
    if (code >= 66 && code <= 67)   return "FrzgRain";
    if (code >= 71 && code <= 75)   return "Snowy";
    if (code == 77)                 return "SnwGrain";
    if (code >= 80 && code <= 82)   return "Showers";
    if (code >= 85 && code <= 86)   return "SnwShwer";
    if (code >= 95 && code <= 99)   return "Thunder";
    return "Unknown";
}

// ---------------------------------------------------------------------------
// fetchWeather — Open-Meteo current_weather endpoint (no API key)
// URL structure: ?current_weather=true returns temperature + weathercode
// Elevation is automatically interpolated by Open-Meteo from DEM data.
// ---------------------------------------------------------------------------
void ClockService::fetchWeather() {
    if (!online) return;

    unsigned long now = millis();
    if (lastWeatherAttempt != 0 && (now - lastWeatherAttempt) < WEATHER_MIN_FETCH_INTERVAL_MS) {
        EVA_LOGLN("[ClockService] Weather rate-limited, skipping");
        return;
    }
    lastWeatherAttempt = now;

    // current_weather=true → { current_weather: { temperature, weathercode, ... } }
    // Open-Meteo automatically uses DEM elevation for the given coordinates —
    // no explicit elevation parameter needed (and &elevation=auto is invalid).
    String url = "http://api.open-meteo.com/v1/forecast"
                 "?latitude="  WEATHER_LATITUDE
                 "&longitude=" WEATHER_LONGITUDE
                 "&current_weather=true"
                 "&timezone=auto";

    EVA_LOGF("[ClockService] Weather URL: %s\n", url.c_str());

    WiFiClient client;
    HTTPClient http;
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    http.setTimeout(HTTP_TIMEOUT_MS);

    if (!http.begin(client, url)) {
        EVA_LOGLN("[ClockService] http.begin() failed");
        return;
    }

    int httpCode = http.GET();
    EVA_LOGF("[ClockService] Weather HTTP %d\n", httpCode);

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        EVA_LOGF("[ClockService] Payload: %.160s\n", payload.c_str());

        StaticJsonDocument<512> doc;
        DeserializationError err = deserializeJson(doc, payload);
        if (!err && doc.containsKey("current_weather")) {
            JsonObject cw = doc["current_weather"];
            float tempC = cw["temperature"] | NAN;
            int   wcode = cw["weathercode"]  | -1;

            if (!isnan(tempC)) {
                // CP437 character 0xF8 = ° on SSD1306/Adafruit GFX
                char buf[16];
                snprintf(buf, sizeof(buf), "%.1f%cC", tempC, (char)0xF8);
                weatherSummary = String(buf);
            } else {
                weatherSummary = "N/A";
            }

            weatherCondition = (wcode >= 0) ? weatherCodeToCondition(wcode) : "Unknown";

            EVA_LOGF("[ClockService] Weather OK: %s %s (WMO code %d)\n",
                     weatherSummary.c_str(), weatherCondition.c_str(), wcode);
        } else {
            EVA_LOGF("[ClockService] JSON error: %s\n", err.c_str());
            weatherSummary   = "N/A";
            weatherCondition = "Unknown";
        }
    } else {
        EVA_LOGF("[ClockService] HTTP error %d\n", httpCode);
        weatherSummary   = "Offline";
        weatherCondition = "Offline";
    }
    http.end();
    lastWeatherFetch = millis();
}

// ---------------------------------------------------------------------------
// Getters
// ---------------------------------------------------------------------------
String ClockService::getWeatherSummary() const {
    return weatherSummary.length() ? weatherSummary : "Offline";
}

String ClockService::getWeatherCondition() const {
    return weatherCondition.length() ? weatherCondition : "Offline";
}
