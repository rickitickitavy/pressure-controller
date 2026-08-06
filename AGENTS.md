# AGENTS.md — pressure-controller

ESP32-C3 pump pressure controller (PlatformIO / Arduino). Repo path: `presure-controller` (typo in folder name is intentional — do not rename casually).

## Before changing firmware

1. Read and follow [`.cursor/skills/esp32/SKILL.md`](.cursor/skills/esp32/SKILL.md).
2. For SPI, EEPROM, OTA, or callbacks, also read [`.cursor/skills/esp32/patterns.md`](.cursor/skills/esp32/patterns.md).
3. For product/runtime context, use [`docs/HANDOFF.md`](docs/HANDOFF.md).

## Always-on project rules

Enforced via [`.cursor/rules/`](.cursor/rules/):

| Rule | Meaning |
|------|---------|
| `esp32.mdc` | Apply the esp32 skill before firmware edits |
| `no-font-upscale.mdc` | No `setTextSize(n≠1)`; use sized GFXfonts; draw bitmaps 1:1 |
| `no-self-instance-ref.mdc` | No ad-hoc `static Foo *instance` in class files; wire callbacks from `main.cpp` |
| `no-project-logic-in-universal.mdc` | No project key/unit special cases inside shared algorithms — put metadata on descriptors / registration |

## Stack

- Board: `esp32-c3-devkitc-02` — see [`platformio.ini`](platformio.ini)
- Pins: [`include/pins.h`](include/pins.h)
- Settings: EEPROM + versioned struct (`GlobalSettings`) — **not** Preferences/NVS
- Web assets: LittleFS under [`data/`](data/); flash with `pio run -t uploadfs`
- Display: ST7789 240×320, Adafruit GFX

## Build / flash

```bash
pio run
pio run -t upload
pio run -t uploadfs    # after data/ changes
pio device monitor     # 115200
```

## Hard constraints (do not violate)

- **ADC:** never `analogRead()` for sensor values — only `analogReadMilliVolts()`.
- **UI:** `setTextSize(1)` with GFXfonts; no bitmap upscaling at draw time.
- **Callbacks:** register handlers from owners (e.g. `main.cpp`), not fake class singletons.
- **Shared code:** extend neutral APIs / descriptor fields; do not add `if (paramName == "...")` lists in universal paths.
- **SPI MISO on C3:** unused MISO cannot be `-1`; use the dummy pin in `pins.h`.

## OTA (two paths)

| Path | When allowed |
|------|----------------|
| ArduinoOTA | AP always; STA only if `network.enableOtaOnNetwork` |
| HTTP `/update/code`, `/update/data` | Whenever web UI is reachable; **ignores** `enableOtaOnNetwork` |

Skip `ArduinoOTA.handle()` while an HTTP upload is in progress.

## Web UI

- Left nav: **Status** / **Settings** / **Update** — [`data/index.html`](data/index.html)
- Status polls `GET /statusApi` every 1 s **only while Status is visible**
- Enable toggle: `POST /statusApi?operation=write&enabled=0|1` (same path as MQTT `on`/`off`)
- Settings: existing sub-tabs (WiFi, Display, MQTT, Pressure, Sensor) + `/settingsApi`
- Update: Code = `firmware.bin` (`U_FLASH`); Data = `littlefs.bin` (`U_SPIFFS`)
- Version label: `FIRMWARE_VERSION` in [`src/Defines.h`](src/Defines.h)

## Key sources

| Area | Path |
|------|------|
| FSM / control / OTA lifecycle | `src/main.cpp` |
| HTTP + status + HTTP OTA | `src/WebServerController.*` |
| MQTT | `src/MqttClient.*` |
| Settings / EEPROM | `src/SettingsManager.*`, `src/GlobalSettings.h`, `src/SettingsNavigator.*` |
| WiFi STA/AP | `src/WiFiController.*` |
| Display | `src/display.cpp`, `include/display.h` |
| Pressure ADC | `src/pressure.cpp`, `include/pressure.h` |

## Working style

- Prefer small, focused diffs; match existing naming and patterns.
- Do not commit unless the user asks.
- After `data/` changes, remind to run `uploadfs` as well as firmware upload.
- Keep [`docs/HANDOFF.md`](docs/HANDOFF.md) in sync when behavior or APIs change.
