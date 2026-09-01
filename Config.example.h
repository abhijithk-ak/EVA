#pragma once
/*
 * Config.example.h
 * Copy this file to Config.h and set your real values locally.
 * Do not commit Config.h or any credential-bearing file.
 */

#include <Arduino.h>

#define EVA_SCREEN_WIDTH   128
#define EVA_SCREEN_HEIGHT  64
#define EVA_OLED_RESET     -1
#define EVA_OLED_I2C_ADDR  0x3C

#define EVA_I2C_SDA        21
#define EVA_I2C_SCL        22

#define PIN_MOTOR_LF       25
#define PIN_MOTOR_LB       26
#define PIN_MOTOR_RB       32
#define PIN_MOTOR_RF       33

#define PWM_FREQ_HZ        20000
#define PWM_RESOLUTION_BIT 8
#define PWM_CH_LF          0
#define PWM_CH_LB          1
#define PWM_CH_RB          2
#define PWM_CH_RF          3

#define MOVE_DEFAULT_SPEED       200
#define MOVE_TURN_SPEED          190
#define MOVE_FORWARD_MS          700
#define MOVE_TURN_MS             450
#define MOVE_BACK_MS             500
#define MOVE_AVOID_TURN_MS       600
#define MOVE_STOP_SETTLE_MS      150

#define TOF_OBSTACLE_MM           95
#define TOF_EDGE_MM               150
#define TOF_POLL_INTERVAL_MS      60
#define TOF_TIMEOUT_MS            50
#define TOF_EDGE_CONFIRM_SAMPLES   3
#define TOF_OBSTACLE_CONFIRM_SAMPLES 3

#define PIN_TOUCH                 4
#define TOUCH_POLL_INTERVAL_MS    15
#define TOUCH_BASELINE_ALPHA      0.005f
#define TOUCH_TRIGGER_RATIO       0.78f
#define TOUCH_TRIGGER_DELTA       200
#define TOUCH_TAP_MAX_MS           800
#define TOUCH_DOUBLE_TAP_WINDOW_MS 400
#define TOUCH_PET_MIN_MS          1100
#define TOUCH_PET_MAX_MS          5000
#define TOUCH_LONG_HOLD_MS        5000

#define PIN_MOOD_LED               27
#define MOOD_LED_COUNT             1
#define MOOD_LED_BRIGHTNESS        1

#define PIN_BUZZER                 14
#define PWM_CH_BUZZER               4
#define BUZZER_VOLUME               250
#define DISPLAY_BRIGHTNESS          250

#define PIN_SERVO                  13
#define SERVO_ENABLED              false

#define BEHAVIOUR_REACTION_DWELL_MIN_MS    3000
#define BEHAVIOUR_REACTION_DWELL_MAX_MS    9000

#define ENERGY_DECAY_RATE_PER_S      0.00150f
#define ENERGY_SLEEP_THRESHOLD       0.15f
#define ENERGY_WAKE_BOOST            0.85f
#define CURIOSITY_RISE_RATE_PER_S    0.008f
#define CURIOSITY_SATISFY_RATE_PER_S 0.015f
#define SOCIAL_RISE_RATE_PER_S       0.002f
#define COMFORT_RESTORE_RATE         0.015f

#define LIFECYCLE_CATEGORY_MIN_MS        45000UL
#define LIFECYCLE_CATEGORY_MAX_MS       180000UL
#define LIFECYCLE_EMOTION_BASE_MIN_MS    15000UL
#define LIFECYCLE_EMOTION_BASE_MAX_MS    60000UL
#define MOTIVATION_SPIKE_CHECK_MS       8000UL

#define BEHAVIOUR_SLEEPY_AFTER_MIN_MS    5000UL
#define BEHAVIOUR_SLEEPY_AFTER_MAX_MS   10000UL

#define SLEEP_CLOCK_PEEK_INTERVAL_MS  300000UL
#define SLEEP_ANIM_PEEK_INTERVAL_MS   600000UL
#define SLEEP_PEEK_DURATION_MS        8000UL
#define SLEEP_AUTO_WAKE_MIN_MS        180000UL
#define SLEEP_AUTO_WAKE_MAX_MS        300000UL

#define WIFI_SSID                   "YOUR_WIFI_SSID"
#define WIFI_PASSWORD               "YOUR_WIFI_PASSWORD"
#define WIFI_CONNECT_TIMEOUT_MS      8000
#define NTP_SERVER_1                "pool.ntp.org"
#define NTP_SERVER_2                "time.nist.gov"
#define GMT_OFFSET_SEC               19800
#define DAYLIGHT_OFFSET_SEC          0
#define WEATHER_REFRESH_INTERVAL_MS  3600000UL
#define WEATHER_MIN_FETCH_INTERVAL_MS  300000UL
#define WEATHER_LATITUDE            "0.00"
#define WEATHER_LONGITUDE           "0.00"
#define WEATHER_LOCATION_NAME        "Your Location"

#define BT_DEVICE_NAME                "EVA"

#define EVA_DEBUG                  1
#define EVA_SERIAL_BAUD            115200
