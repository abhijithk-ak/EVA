#pragma once
#include <Arduino.h>

// 8x8 1-bit icons for ModeClock UI.
// Edit freely — an easy way to design your own is the "image2cpp" web tool
// (draw an 8x8 grid, export as Arduino code, "Horizontal - 1 bit per pixel").

// Alarm bell
static const unsigned char PROGMEM icon_alarm[] = {
    0x3C, 0x7E, 0x7E, 0x7E, 0x7E, 0xFF, 0x3C, 0x18
};

// WiFi arcs
static const unsigned char PROGMEM icon_wifi[] = {
    0x00, 0x3C, 0x42, 0x18, 0x24, 0x18, 0x18, 0x00
};

// Calendar / date box
static const unsigned char PROGMEM icon_calendar[] = {
    0xFF, 0x81, 0xA9, 0x81, 0xA9, 0x81, 0xA9, 0xFF
};

// Cloud (weather)
static const unsigned char PROGMEM icon_cloud[] = {
    0x00, 0x18, 0x3C, 0x7E, 0xFF, 0xFF, 0x7E, 0x00
};

// Bluetooth symbol  (stylised "B" lozenge)
static const unsigned char PROGMEM icon_bluetooth[] = {
    0x18, 0x1C, 0x5A, 0x3C, 0x3C, 0x5A, 0x1C, 0x18
};

// Rain drops (three vertical lines of dots)
static const unsigned char PROGMEM icon_rain[] = {
    0x00, 0xFF, 0xFF, 0x7E, 0x00, 0x49, 0x49, 0x00
};

// Sun (circle with short rays)
static const unsigned char PROGMEM icon_sun[] = {
    0x42, 0x24, 0x18, 0xFF, 0xFF, 0x18, 0x24, 0x42
};

// Thunder/lightning bolt (for storms)
static const unsigned char PROGMEM icon_thunder[] = {
    0x1C, 0x3C, 0x7C, 0xFE, 0x3C, 0x38, 0x30, 0x00
};

// Snowflake
static const unsigned char PROGMEM icon_snow[] = {
    0x08, 0x49, 0x2A, 0x1C, 0x1C, 0x2A, 0x49, 0x08
};

// No-signal / disconnected indicator (8x8 diagonal X)
// Used in place of WiFi icon when offline, and BT icon when disconnected.
static const unsigned char PROGMEM icon_no_signal[] = {
    0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81
};

