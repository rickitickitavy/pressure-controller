---
name: esp32
description: >-
  Applies ESP32 PlatformIO Arduino conventions: GFX UI rules, callback ownership, 
  pins layout, LittleFS/OTA/EEPROM patterns, and ADC via analogReadMilliVolts only.
  Use when working on ESP32, ESP32-C3, PlatformIO, platformio.ini, LittleFS, ArduinoOTA,
  ADC sensors, ST7789/Adafruit GFX, WiFi/MQTT firmware, or when the user mentions the esp32
  skill.
---

# ESP32 (project conventions)

Rules below are from the pressure-controller stack only. Do not invent generic ESP32 guidance beyond them.

## ADC (hard rule)

- **NEVER** use `analogRead()` for ADC conversion or scaling.
- **ALWAYS** use `analogReadMilliVolts()` for readings used to compute sensor values.
- Match this project’s setup when editing ADC code: `analogReadResolution(12)`, `analogSetAttenuation(ADC_11db)`, multi-sample average when reading pressure.

## UI (GFX / ST7789)

- Never use `setTextSize(n)` with `n != 1` to enlarge text.
- Choose a properly sized Adafruit GFX font; keep `setTextSize(1)` when using GFXfonts.
- Never scale bitmap icons at draw time; draw native-resolution bitmaps 1:1.
- Prefer partial TFT bar redraws in normal updates; no full-screen clear in the hot path.

## Class instance references

- **Not allowed:** ad-hoc `static Foo *instance` inside a class’s `.h`/`.cpp` only so a static/C callback can reach `this`.
- **Allowed:** intentional logical singletons (e.g. `LOGGER`).
- Wire ownership and callbacks from outside the class (e.g. `main.cpp`).

## Layout and stack

- Pins as `constexpr` in `include/pins.h`.
- PlatformIO `espressif32` + Arduino; use the project’s `board` and `build_flags` from `platformio.ini` (including CDC flags when present).
- Filesystem: LittleFS (`board_build.filesystem = littlefs`); web assets under `data/`.
- Persistence: EEPROM + versioned settings struct — **not** Preferences/NVS; do not switch casually.

## Build / flash

```bash
pio run
pio run -t upload
pio run -t uploadfs    # after data/ changes
pio device monitor     # match monitor_speed in platformio.ini
```

## OTA

- ArduinoOTA always while AP is up.
- On STA: only when `network.enableOtaOnNetwork` is true and WiFi is connected.
- Call `ArduinoOTA.handle()` in `loop` while OTA is active.

## Hardware gotcha (ESP32-C3 SPI)

- Unused MISO cannot be `-1` (SPI HAL rejects it); use a real dummy pin as in `pins.h`.

## Deeper notes

- For EEPROM/OTA/SPI detail, see [patterns.md](patterns.md).
