# EVA — Behaviour-Based Desktop Companion Robot

Release v1.2

EVA is a small ESP32-powered desktop companion robot designed to feel alive through behaviour, emotion, movement, lighting, and sound rather than through random motion or simple obstacle avoidance.

The project follows a strict data flow:

```text
Sensor → Interpretation → Behaviour → Emotion → Expression / Movement / Light / Sound
```

This repository contains the full firmware and supporting project files for the EVA robot, along with the deeper implementation notes in [PROJECT_DOCUMENTATION.md](./PROJECT_DOCUMENTATION.md).

---

## What EVA does

- Shows an expressive OLED face using RoboEyes
- Reacts to touch, proximity, and sound
- Cycles through a personality-driven idle and curiosity loop
- Changes mood and expression based on sensor events
- Switches between autonomous, pet, clock, sleep, and remote-control modes
- Uses non-blocking timing throughout the runtime logic

---

## Core features

- 8 EVA-specific emotional states: Neutral, Happy, Very Happy, Curious, Scared, LOL, Angry, Sleepy
- Central behaviour engine that decides how EVA responds to the world
- Autonomous mode with idle behaviour, curiosity, obstacle avoidance, and edge reactions
- Pet mode for gentle interaction without driving movement
- Clock mode with local time, alarm support, weather, and quote display
- Sleep mode with minimal output and wake-up on touch
- RC mode using ESP32 classic Bluetooth
- WS2812 mood lighting and passive buzzer cues
- Sensor interpretation layer for ToF, touch, and microphone data

---

## Hardware

EVA is built around an ESP32 DevKit/WROOM and the following modules:

- 0.96" SSD1306 OLED display
- FluxGarage RoboEyes eyes renderer
- 2x N20 DC motors driven by a DRV8833
- VL53L0X time-of-flight distance sensor
- Capacitive touch plate
- MAX9814 microphone module
- WS2812 LED mood light
- Passive buzzer

Full pinout, BOM, and calibration notes are documented in [PROJECT_DOCUMENTATION.md](./PROJECT_DOCUMENTATION.md).

---

## Modes

| Mode | Purpose | Notes |
|---|---|---|
| EVA | Autonomous companion life | Full behaviour engine, movement, reaction system |
| Pet | Touch-first interaction | Movement disabled; interaction logic remains active |
| Clock | Display and alarm mode | Wi-Fi optional, weather and quotes can be enabled |
| Sleep | Rest state | Minimal output, wake on touch |
| RC | Bluetooth driving | Manual control using classic Bluetooth RC protocols |

---

## Software architecture

The firmware is intentionally structured around a small set of clear responsibilities:

```text
ModeEngine
  ├─ ModeEva
  ├─ ModePet
  ├─ ModeClock
  ├─ ModeSleep
  └─ ModeRC

BehaviourEngine
  ├─ EmotionEngine
  ├─ MovementEngine
  ├─ MoodLightManager
  ├─ BuzzerManager
  └─ Sensor / touch / sound interpretation
```

Important design points:

- No runtime `delay()` calls are used for the main behaviour loop
- Sensor managers interpret input before behaviour logic consumes it
- Behaviour is centralized to keep reactions consistent across modes
- The robot architecture is built for clearer debugging and future expansion

---

## Project structure

```text
EVA/
├── EVA.ino                 # setup()/loop() and object wiring
├── Config.h                # pins, thresholds, and runtime settings
├── Types.h                 # shared enums and state definitions
├── EmotionEngine.h         # EVA expression logic
├── BehaviourEngine.h       # central behaviour/state machine
├── MovementEngine.h/.cpp   # motor control
├── SensorManager.h/.cpp    # proximity sensing
├── TouchManager.h/.cpp     # touch interpretation
├── SoundManager.h/.cpp     # microphone events
├── MoodLightManager.h/.cpp # LED mood effects
├── BuzzerManager.h/.cpp    # buzzer sound sequencing
├── ClockService.h/.cpp     # time, alarm, weather, quote handling
├── Mode.h                  # mode interface
├── ModeEngine.h            # active mode manager
├── ModeEva.h               # autonomous mode
├── ModePet.h               # pet mode
├── ModeClock.h/.cpp        # clock mode
├── ModeSleep.h             # sleep mode
├── ModeRC.h                # remote-control mode
├── CommsHub.h/.cpp         # Bluetooth / command handling
├── README.md               # project overview
├── PROJECT_DOCUMENTATION.md # detailed architecture and calibration guide
└── Config.example.h        # sample configuration template
```

---

## Getting started

1. Install the ESP32 board package in the Arduino IDE.
2. Install the required libraries:
   - Adafruit GFX
   - Adafruit SSD1306
   - FluxGarage RoboEyes
   - Adafruit VL53L0X
   - Adafruit NeoPixel
   - ArduinoJson
3. Keep all project files in a single sketch folder named `EVA` and open `EVA.ino`.
4. Copy `Config.example.h` to `Config.h` and adjust Wi-Fi, location, and timing values as needed.
5. Upload the firmware to the ESP32.
6. Open the Serial Monitor at 115200 baud for runtime logs and mode commands.

---

## Build and flash notes

The project is designed for the Arduino IDE and the ESP32 core. Clock mode can run without Wi-Fi, but weather and quote features require valid network settings in `Config.h`.

For full wiring, calibration, and troubleshooting details, see [PROJECT_DOCUMENTATION.md](./PROJECT_DOCUMENTATION.md).

---

## What's new in v1.2

This release includes the stability and usability fixes that were noted during the v1.2 implementation pass:

- unified one-channel Bluetooth control for mode switching, alarm setup, time commands, and RC driving
- safer RC parsing so a single raw control byte is treated as an immediate command instead of being misclassified as a text line
- Bluetooth safety watchdog to stop EVA if the RC link drops mid-drive
- sleep-mode eye rendering fix so sleepy closed eyes remain stable instead of reopening unexpectedly
- touch event consumption fix to prevent repeated tap/petting events from being replayed across loops
- alarm setup adjustments to make tap-driven hour/minute editing advance by one logical step instead of skipping values
- stronger mode transition handling through the shared mode engine and more predictable wake/sleep flow

## Status: v1.2

This release represents the complete v1.2 feature set:

- autonomous behaviour loop
- touch and sound response system
- proximity and edge awareness
- EVA emotion engine
- clock and alarm support
- sleep and wake transitions
- RC driving via Bluetooth
- Bluetooth safety and command handling improvements
- mood light and buzzer feedback

The project is ready as a working desktop companion robot prototype for experimentation, refinement, and extension.

---

## Known limitations

- Motion is open-loop and not encoder-based
- Sound interpretation is coarse pattern detection rather than speech recognition
- Touch and distance thresholds may need calibration on a specific chassis
- The system is intended as a personality-driven prototype, not a commercial-grade product platform

---

## Credits

- [FluxGarage RoboEyes](https://github.com/FluxGarage/RoboEyes) for the expression renderer
- [Open-Meteo](https://open-meteo.com/) for weather data
- [ZenQuotes](https://zenquotes.io/) for quote data

---

## License

This project is provided as an open firmware project for experimentation and personal use. Check the repository policy and upstream library licenses before commercial reuse or redistribution.
