# EVA — Behaviour-Based Desktop Companion Robot

**Full Project Documentation**
Version: 0.9.0 (Stages 0.1 – 1.2 implemented) · Platform: ESP32 DevKit / ESP32-WROOM · IDE: Arduino IDE

---

## 1. What EVA Is

EVA is a small desktop companion robot whose behaviour is designed to feel **intentional, reactive, curious, and alive** — not an obstacle-avoiding robot, not a voice assistant, and not a pile of random motor twitches.

Every output EVA produces (movement, expression, light, sound) is the result of a strict one-way pipeline:

```
Sensor → Interpretation → Behaviour → Emotion → Expression / Movement / Light / Sound
```

No sensor is ever allowed to trigger a motor, a mood, or a light directly. That rule is enforced structurally by the code architecture, not just by convention — see §3.

---

## 2. Hardware

| Component | Part | Notes |
|---|---|---|
| Controller | ESP32 DevKit / ESP32-WROOM | Classic ESP32 (needed for BluetoothSerial in RC mode) |
| Display | 0.96" OLED SSD1306, 128×64, I2C | Adafruit_GFX + Adafruit_SSD1306 |
| Eyes renderer | FluxGarage RoboEyes | Used unmodified, official API only |
| Motors | 2× DC N20 geared, no encoders | Driven via DRV8833 |
| Motor driver | DRV8833 | 4 direction/PWM pins |
| Spatial sensor | VL53L0X ToF | Mounted at a downward angle |
| Touch | Capacitive/metal plate | ESP32 `touchRead()`, GPIO4 |
| Microphone | MAX9814 | Analog envelope out, environmental awareness only |
| Mood light | WS2812 (NeoPixel) | 1 LED by default, expandable |
| Buzzer | Passive buzzer | `tone()`/`noTone()` driven |
| Optional | SG90 servo | Not wired by default (`SERVO_ENABLED = false` in Config.h) |

### 2.1 Pinout (ESP32 DevKit)

| Signal | GPIO | Notes |
|---|---|---|
| I2C SDA (OLED + VL53L0X) | 21 | Shared bus, different addresses |
| I2C SCL | 22 | |
| Motor: Left Forward | 25 | PWM (LEDC channel 0) |
| Motor: Left Backward | 26 | PWM (LEDC channel 1) |
| Motor: Right Backward | 32 | PWM (LEDC channel 2) |
| Motor: Right Forward | 33 | PWM (LEDC channel 3) |
| Touch plate | 4 | `touchRead()` capable (T0) |
| Microphone (MAX9814 OUT) | 34 | ADC1, input-only pin |
| Mood light (WS2812 DIN) | 27 | |
| Buzzer | 14 | |
| Servo (optional) | 13 | Disabled by default |

All pins are centralised in `Config.h` — nothing else in the firmware hard-codes a GPIO number.

### 2.2 Calibration required before first use

- **VL53L0X thresholds** (`TOF_OBSTACLE_MM` = 95, `TOF_EDGE_MM` = 150 in `Config.h`) depend entirely on the sensor's mounting angle and desk surface. Recalibrate on the physical chassis.
- **Touch baseline** is dynamic (an exponential moving average seeded at boot), but the trigger ratio (`TOUCH_TRIGGER_RATIO`) and idle reading may need adjusting per touch plate/material.
- **Microphone thresholds** (`MIC_LOUD_RATIO`) depend on the MAX9814 gain setting and room acoustics.

---

## 3. Software Architecture

```
                    ┌─────────────────┐
                    │    ModeEngine   │   decides WHICH mode is active
                    └────────┬────────┘
                             │
              ┌──────────────┼──────────────┬───────────────┬───────────────┐
              ▼              ▼              ▼               ▼               ▼
          ModeEva         ModePet       ModeClock        ModeSleep       ModeRC
              │              │
              └──────┬───────┘
                     ▼
             ┌─────────────────┐
             │ BehaviourEngine │   the ONLY place sensors become actions
             └────────┬────────┘
                       │
            ┌──────────┴──────────┐
            ▼                     ▼
    ┌──────────────┐      ┌──────────────┐
    │EmotionEngine │      │MovementEngine│
    └──────┬───────┘      └──────────────┘
           │
 ┌─────────┼──────────┐
 ▼         ▼          ▼
RoboEyes  MoodLight  Buzzer
   │
   ▼
  OLED
```

Sensors feed **interpretation-only** managers, never behaviour directly:

```
VL53L0X ──► SensorManager  ──┐
Touch   ──► TouchManager   ──┼──► BehaviourEngine
MAX9814 ──► SoundManager   ──┘
```

### 3.1 File map

| File | Responsibility |
|---|---|
| `EVA.ino` | Hardware bring-up, object wiring, `setup()`/`loop()` only |
| `Config.h` | Every pin, threshold, and timing constant in the project |
| `Types.h` | Shared enums (`EvaEmotion`, `EvaBehaviour`, `EvaMode`, `MoveState`, sensor events) |
| `Logger.h` | Zero-cost-when-disabled debug logging |
| `EmotionEngine.h` | Combines official RoboEyes primitives into EVA emotions |
| `BehaviourEngine.h` | Central "alive" state machine — idle/curiosity cycle + reactions |
| `MovementEngine.h/.cpp` | Non-blocking, timed, open-loop motor control |
| `SensorManager.h/.cpp` | VL53L0X → `SpatialEvent` (CLEAR/OBSTACLE/EDGE) |
| `TouchManager.h/.cpp` | Capacitive touch → `TouchEvent` (TAP/PETTING/LONG_HOLD), dynamic baseline |
| `SoundManager.h/.cpp` | MAX9814 envelope → `SoundEvent` (CLAP/LOUD_NOISE/MUSIC_LIKE) |
| `MoodLightManager.h/.cpp` | WS2812 solid/pulse effects, emotion → colour mapping |
| `BuzzerManager.h/.cpp` | Non-blocking note-sequence player, emotion → sound mapping |
| `ClockService.h/.cpp` | Wi-Fi, NTP time, alarm, weather (Open-Meteo), quote (ZenQuotes), offline fallback |
| `Mode.h` | Abstract interface implemented by all 5 modes |
| `ModeEngine.h` | Owns the active mode, applies global transition rules |
| `ModeEva.h` | Mode 1 |
| `ModePet.h` | Mode 2 |
| `ModeClock.h/.cpp` | Mode 3 |
| `ModeSleep.h` | Mode 4 |
| `ModeRC.h` | Mode 5 |

### 3.2 Why templates are used (and where)

`RoboEyes<Display>` is itself a template from the FluxGarage library. To avoid `EmotionEngine` and `BehaviourEngine` needing to know the concrete display type, both are declared as:

```cpp
EmotionEngine<decltype(roboEyes)>   emotion(roboEyes);
BehaviourEngine<decltype(emotion)>  behaviour(emotion, movement, light, buzzer);
```

Everything downstream of `BehaviourEngine` (movement, light, buzzer, sensors) uses concrete, non-templated classes — templates are used only where the RoboEyes type genuinely needs to be threaded through, keeping the rest of the codebase simple and readable (a stated project goal, since this is also a C++ learning project).

### 3.3 Non-blocking design

No file uses `delay()` in any runtime path. Every timed behaviour (movement duration, temporary emotion effects like sweat/flicker, buzzer note sequences, mood-light pulsing, touch/petting classification, sleep's periodic time display) is implemented as a `millis()`-based timer compared against a stored start time. The only `delay()` calls in the whole project are one-time, boot-time sensor-baseline-seeding calls in `TouchManager::begin()` / `SoundManager::begin()`, which are explicitly commented as such.

---

## 4. Emotion System

EVA does not require RoboEyes to expose a distinct built-in mood for every emotion EVA needs. Instead, `EmotionEngine` combines the **official** RoboEyes API surface — moods, curiosity, sweat, flicker, cyclops, one-shot animations — into EVA-specific emotions:

| EVA Emotion | Built from |
|---|---|
| `EVA_NEUTRAL` | `DEFAULT` mood |
| `EVA_HAPPY` | `HAPPY` mood |
| `EVA_VERY_HAPPY` | `HAPPY` mood + `anim_laugh()` |
| `EVA_CURIOUS` | `DEFAULT` mood + curiosity ON |
| `EVA_SCARED` | `TIRED` mood + timed sweat (5s) + timed flicker (3s) |
| `EVA_LOL` | `HAPPY` mood + cyclops ON + `anim_laugh()` |
| `EVA_ANGRY` | `ANGRY` mood (official RoboEyes mood) |
| `EVA_SLEEPY` | `TIRED` mood |

Persistent base emotion and temporary effects are layered independently (e.g. `SCARED` = persistent `TIRED` + temporary sweat + temporary flicker), each with its own `millis()` timer, so several timed effects can run without blocking or interfering with each other.

`RoboEyes` itself is **never modified** — only its documented public API is called.

---

## 5. Behaviour System

`BehaviourEngine` is the single authority allowed to turn an interpreted sensor event into an emotion or a movement. Its state machine:

```
IDLE  ──(been still for a while, randomised timing)──►  CURIOUS
CURIOUS ──(look, and ~60% of the time move)──► WANDER ──► IDLE
IDLE (still idle for BEHAVIOUR_SLEEPY_AFTER_MS) ──► SLEEPY ──► (Mode switches to Sleep)
```

Reactive events (obstacle, edge, touch, sound) pre-empt this cycle, run for a fixed "busy" window (with an intentional matching emotion + light + sound), and then automatically hand control back to the idle/curious cycle:

| Trigger | Behaviour | Emotion | Movement |
|---|---|---|---|
| Obstacle (< 95 mm) | `OBSTACLE_AVOID` | Scared | Stop, pivot away |
| Edge (> 150 mm / out of range) | `EDGE_AVOID` | Scared | Stop, back away |
| Tap | `TOUCH_REACT` | Happy | — |
| Petting (5–10 s) | `TOUCH_REACT` | Angry | — |
| Long hold (> 20 s) | → Sleep Mode | Sleepy | — |
| Clap | `SOUND_REACT` | Curious | — |
| Loud noise | `SOUND_REACT` | Scared | — |
| Music-like | `SOUND_REACT` | Very Happy | — |

Idle → curiosity timing is **randomised within a controlled range** (`BEHAVIOUR_IDLE_MIN_MS`/`MAX_MS`, `BEHAVIOUR_CURIOUS_MIN_MS`/`MAX_MS` in `Config.h`) rather than fixed, so EVA doesn't behave like a metronome — this directly implements the "next improvement" called out in the original project notes.

`BehaviourEngine` is reused, unmodified, by both Mode 1 (movement enabled) and Mode 2 (movement disabled via `setMovementEnabled(false)`) — this keeps the "alive" logic in exactly one place.

---

## 6. Modes

### Mode 1 — EVA (autonomous life)
Full `BehaviourEngine` active: idle/curiosity cycle, obstacle/edge response, touch reactions, sound reactions, emotional cycling, and automatic hand-off to Sleep after a long period of stillness.

### Mode 2 — Pet Mode
Touch and sound stay active, emotion engine stays active, autonomous movement is disabled. Reuses `BehaviourEngine` with movement turned off, so the touch lifecycle is identical logic to Mode 1's touch reactions:
- Tap / short pet → Happy
- Continuous petting 5–10 s → Angry
- Hold > 20 s → Sleep

### Mode 3 — Clock Mode
Movement fully disabled; this mode takes over the OLED directly (RoboEyes is paused) to draw a simple 12-hour clock, date, alarm status, and — when online — rotating weather/quote info.
- On entry, attempts Wi-Fi automatically (`ClockService::begin()`), with a bounded timeout; falls back to fully offline behaviour if unavailable.
- Weather comes from the free Open-Meteo API (no key required); a quote comes from ZenQuotes, with a small built-in offline quote set as fallback.
- Alarm setup is touch-driven: **pet** to cycle Normal → Set Hour → Set Minute → committed; **tap** changes the field being edited (or toggles the alarm on/off when not editing).
- When the alarm fires, `BuzzerManager::playAlarmBeep()` plays.

### Mode 4 — Sleep Mode
Entered automatically from Mode 1/2's long-idle or long-touch-hold. RoboEyes shows its closed-eyes idle animation. Every 5 minutes, EVA takes over the display to show the time for up to 1 minute, then returns to sleep. A tap at any point wakes EVA back into Mode 1.

### Mode 5 — RC Mode
Manual Bluetooth driving via ESP32 classic Bluetooth (`BluetoothSerial`, advertised as `"EVA-RC"`). Compatible with common single-character "Bluetooth RC Car" Android app protocols out of the box (F/B/L/R/S plus diagonals and a 0–9 speed selector) — no custom app is required to get started. Autonomous behaviour and `BehaviourEngine` are fully disabled; emotion stays at a calm neutral idle. A safety watchdog stops the motors if no command arrives for 1.5 s (protects against a dropped Bluetooth link).

### Mode switching
`ModeEngine` owns the currently active `Mode` and applies the two global rules that don't belong to any single mode (long-hold → Sleep, tap-while-asleep → wake). For bring-up and testing, modes can also be switched explicitly over USB serial:

```
MODE EVA
MODE PET
MODE CLOCK
MODE SLEEP
MODE RC
```

(A physical button or a Bluetooth/app command can call `ModeEngine::switchTo()` directly in a future revision — the hook is already there.)

---

## 7. Build & Flash Instructions

### 7.1 Arduino IDE setup
1. Install the **ESP32 board package** (Arduino IDE → Preferences → Additional Board Manager URLs → add the Espressif package index, then install "esp32" via Boards Manager).
2. Select **Board:** an ESP32 Dev Module (classic ESP32, not S3/C3 — RC Mode needs classic Bluetooth).
3. Select the correct **Port**.

### 7.2 Required libraries (Library Manager)
| Library | Used for |
|---|---|
| Adafruit GFX Library | OLED graphics primitives |
| Adafruit SSD1306 | OLED driver |
| FluxGarage RoboEyes | Eyes renderer (install from [FluxGarage/RoboEyes](https://github.com/FluxGarage/RoboEyes)) |
| Adafruit VL53L0X | Spatial sensor |
| Adafruit NeoPixel | WS2812 mood light |
| ArduinoJson | Parsing weather/quote API responses |
| BluetoothSerial | Bundled with the ESP32 core — no separate install |
| WiFi, HTTPClient | Bundled with the ESP32 core |

### 7.3 Flashing
1. Open `EVA.ino` (keep every file in this documentation's file map inside the same `EVA/` sketch folder — Arduino requires that).
2. Fill in `WIFI_SSID` / `WIFI_PASSWORD` in `Config.h` if you want Clock Mode online (optional — everything else works with these left blank).
3. Set `GMT_OFFSET_SEC` and `WEATHER_LATITUDE`/`WEATHER_LONGITUDE` in `Config.h` for your location.
4. Upload.
5. Open Serial Monitor at 115200 baud to see boot logs and to send `MODE ...` commands.

### 7.4 A note on the ESP32 Arduino core version
`MovementEngine::begin()` uses the classic `ledcSetup()`/`ledcAttachPin()`/`ledcWrite()` API (Arduino-ESP32 core v2.x). If you are on core v3.x, replace those three calls with the newer single-call `ledcAttach(pin, freq, resolutionBits)` + `ledcWrite(pin, duty)` API — the rest of `MovementEngine` is unaffected either way.

---

## 8. User Manual

### 8.1 Everyday use
- Power on EVA — it boots directly into **Mode 1 (EVA)**.
- Leave EVA alone and it will idle, occasionally look around (curiosity), and settle back down — timing is randomised so it never feels mechanical.
- Bring a hand or object close to the front sensor and EVA will react (stop/turn away) and show a startled expression.
- Tap the touch plate for a quick, happy reaction.
- Clap near EVA for a curious reaction; a sudden bang/knock gets a startled reaction; play music nearby for a happy reaction.
- Leave EVA alone long enough (2 minutes of continuous idle) and it will get sleepy and go to sleep on its own; you can also hold the touch plate for 20+ seconds to send it to sleep immediately.
- Tap EVA while it's asleep to wake it back into Mode 1.

### 8.2 Pet Mode
Switch to Pet Mode (`MODE PET` over serial, or your own trigger) when you don't want EVA driving around — e.g. sitting on a desk you don't want it wandering off. Touch reactions and emotions work exactly as in Mode 1; EVA simply won't move.

### 8.3 Clock Mode
Switch to Clock Mode to use EVA as a desk clock with alarm. If Wi-Fi credentials are set in `Config.h`, it will also show a rotating weather reading and short quote. Pet the touch plate to enter alarm setup, tap to adjust, pet again to confirm (or wait 8 seconds and it auto-confirms).

### 8.4 RC Mode
Switch to RC Mode, then pair a Bluetooth RC-car style Android app to **"EVA-RC"** and drive EVA manually. EVA will automatically stop if the Bluetooth link drops.

### 8.5 Calibration checklist (first-time setup)
1. Place EVA on its actual desk/chassis, powered and stationary.
2. Watch Serial output for the seeded touch and microphone baselines.
3. Trigger the touch plate and confirm TAP/PETTING/LONG_HOLD are detected as expected; adjust `TOUCH_TRIGGER_RATIO` if needed.
4. Wave a hand in front of the VL53L0X at your intended "too close" distance and confirm an OBSTACLE reaction; adjust `TOF_OBSTACLE_MM`.
5. Move EVA to the edge of the desk and confirm an EDGE reaction; adjust `TOF_EDGE_MM`.
6. Clap/knock near the microphone and confirm reactions fire without excessive false positives; adjust `MIC_LOUD_RATIO`.

---

## 9. Known Limitations

- **No encoders** on the drive motors: all movement is timed and open-loop, not precision navigation. Distance/rotation are approximate.
- **VL53L0X mounted downward** must serve double duty (obstacle + edge); the two thresholds require physical calibration and can conflict on unusual surfaces.
- **MAX9814 sound detection** is coarse, threshold/peak-based pattern matching — not real audio classification, and there is no speech recognition of any kind.
- **Touch readings** vary with environment, humidity, and the specific plate material; the dynamic baseline mitigates but does not eliminate this.
- **Clock Mode's online features** (weather/quote) depend on network availability and free third-party APIs (Open-Meteo, ZenQuotes) that may change or rate-limit; offline defaults always keep the clock itself functional.
- **RC Mode requires classic Bluetooth**, so it will not run on ESP32-S3/C3 boards (BLE-only) — use a classic ESP32/ESP32-WROOM.
- **Sleep Mode's periodic time display** relies on NTP time already having been synced (i.e. Clock Mode having been entered at least once since boot, or your own explicit `ClockService::begin()` call at startup); without that it shows a placeholder.

---

## 10. Development Roadmap Status

| Stage | Scope | Status |
|---|---|---|
| 0.1 | Expression foundation (EmotionEngine, RoboEyes integration, temporary effects, basic behaviour state machine) | ✅ |
| 0.2 | Behaviour (randomised idle timing, curiosity events, return-to-idle logic) | ✅ |
| 0.3 | Movement (MovementEngine, non-blocking states, timed movement, safe stop) | ✅ |
| 0.4 | Spatial awareness (VL53L0X obstacle/edge interpretation) | ✅ |
| 0.5 | Touch (dynamic baseline, tap/pet/hold, emotion transitions) | ✅ |
| 0.6 | Sound (MAX9814 event detection) | ✅ |
| 0.7 | Mood light + audio (WS2812, passive buzzer) | ✅ |
| 0.8 | EVA Mode 1 (full autonomous lifecycle) | ✅ |
| 0.9 | Pet Mode | ✅ |
| 1.0 | Clock Mode | ✅ |
| 1.1 | Sleep Mode | ✅ |
| 1.2 | RC Mode | ✅ |

### Suggested next steps for a production/commercial revision
- Replace the serial-command mode switch with a physical button, gesture, or companion-app command.
- Add persistent storage (`Preferences`/NVS) for the alarm setting and calibrated thresholds so they survive reboot.
- Add a proper enclosure-aware low-battery/voltage-monitor behaviour if run from a battery.
- Consider a small companion mobile app in place of a generic RC app for a polished product experience.
- Expand the WS2812 mood light to a short ring/strip for richer emotional lighting patterns.

---

## 11. Reference Links

- FluxGarage RoboEyes (official): https://github.com/FluxGarage/RoboEyes
- FluxGarage GitHub: https://github.com/FluxGarage
- Project repository: https://github.com/abhijithk-ak/EVA
