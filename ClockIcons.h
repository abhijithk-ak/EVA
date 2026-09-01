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
    0x00, 0x30, 0x7C, 0xFE, 0xFE, 0x7C, 0x00, 0x00
};
