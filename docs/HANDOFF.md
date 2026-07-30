# Pump controller — handoff / context

PlatformIO project for **ESP32-C3-DevKitC-02** (`espressif32` / Arduino).  
Path: `/home/dsporynkhin/Projects/cpp/home/presure-controller`  
Branch in progress: `feature/add-mqtt-connection`

Use this file plus the plan copies in `docs/` when starting a new chat (`@docs/HANDOFF.md`).

## Hardware (current)

| Function | GPIO |
|----------|------|
| Pressure sensor (ADC) | 0 |
| Encoder A | 10 |
| Encoder B | 1 |
| Encoder button | 8 |
| Pump | 9 |
| ST7789 MOSI | 6 |
| ST7789 SCLK | 4 |
| ST7789 MISO | 5 (unused; SPI HAL rejects `-1`) |
| ST7789 CS | 7 |
| ST7789 DC | 2 |
| ST7789 RST | 3 |

- Display: **240×320**, rotation **180°** (`setRotation(2)`), no backlight pin.
- Pins defined in [`include/pins.h`](../include/pins.h).
- UI rule: never upscale fonts/icons (`setTextSize(1)` + native GFXfonts / 1:1 bitmaps). See `.cursor/rules/no-font-upscale.mdc`.

## Pressure scale

- ADC **0** → **-1** (vacuum / 0 psi).
- ADC **4095** → `sensorMaxMpa` (configurable in SETTINGS, default 0.5 MPa = 5 Atm).
- Display: atmospheres via `mpa / 0.1` (1 Atm = 0.1 MPa). No unit text on LCD.
- Defaults / clamps: [`src/GlobalSettings.h`](../src/GlobalSettings.h).

## Main UI (top → bottom)

Band layout in [`src/display.cpp`](../src/display.cpp) (`kHMax` / `kYMac`+`kHMac` / `kHCurrent` / `kHPump` / `kHMin`):

1. **MAX** — yellow on red (edit: black on yellow); FreeSansBold 24pt  
2. **MAC** (AP mode only) — device MAC (`AA:BB:CC:DD:EE:FF`), white on black, centered; FreeSansBold **12pt**; strip at `kYMac=57` (5px gap under MAX), height 24px; blank when not AP  
3. **Current** — cyan on black, **horizontally centered**; FreeSansBold 24pt  
4. **Pump** — icon (not ON/OFF text); **green** when ON, **gray** when OFF  
5. **WiFi** — blue icon to the right of the pump when STA connected or AP active; label **AP** under the icon in AP mode; label **OTA** under the icon when OTA is active (below **AP** when both)  
6. **MIN** — white on dark green (edit: black on yellow); FreeSansBold 24pt  

`UiState.macAddress` is filled from `WiFi.macAddress()` in `main.cpp` only while `apMode` is true.

Partial bar redraws only (no full-screen clear in normal updates).

## Modes

| Mode | How |
|------|-----|
| Run | Default control |
| Edit MAX / MIN | Short button: Run → EditMax → EditMin → Run; rotate ±0.001 MPa; idle **5 s** (button or rotate resets timer) |
| SETTINGS | Hold button **≥ 3 s** |
| Fail | **LEAK** timeout only; pump off; locked until reboot |

### Fail-safe / pump limits

When the pump turns **ON**:

1. **LEAK** (reach-min): if pressure has not reached **min** within `leakDetectSec` → enter **Fail** (latched until reboot).
2. **WEAK** (max continuous run): if pump stays ON longer than `pumpWeakSec` → **pump OFF only** (no Fail). May turn ON again on the next loop if pressure is still below min (fresh weak timer).

### SETTINGS screen (LCD)

Rows: **LEAK**, **WEAK**, **SENS**, **SAVE**, **CANCEL**

- Short press: cycle focus  
- Rotate: change LEAK / WEAK / SENS  
- Hold **1 s** on SAVE / CANCEL: activate  
- Idle **10 s**: CANCEL  

| Setting | Range | Step | Default |
|---------|-------|------|---------|
| LEAK (reach min after pump ON) | 5–40 s | 1 s | 10 |
| WEAK (max pump ON; then OFF, no Fail) | 40–600 s | 1 s | 180 |
| SENS (sensor full-scale) | 2.0–50.0 Atm | 0.1 Atm | 5.0 |

## Persistence & settings model

- Stored in **EEPROM** (4096 bytes) via [`src/SettingsManager.cpp`](../src/SettingsManager.cpp) — **not** `Preferences`/NVS.
- Schema: [`src/GlobalSettings.h`](../src/GlobalSettings.h) — `NetworkSettings` (incl. `enableOtaOnNetwork`), MQTT fields, `PressureSettings` (`minMpa` / `maxMpa` / `leakDetectSec` / `pumpWeakSec` / `sensorMaxMpa` as `float`/`int`), version/marker (`GLOBAL_CURRENT_SETTINGS_VERSION = 3`). No brightness field.
- First boot (invalid marker): single `SettingsManager::applyDefaults()` fills all defaults, then save + restart.
- Version mismatch: upgrade branch kept (log + stamp version + save) but **no field migrations** yet.
- Web/API parameter names: [`src/SettingsNavigator.cpp`](../src/SettingsNavigator.cpp) + `ParamDescriptor` (incl. `BOOLEAN`, pressure `*`, `network>enableOtaOnNetwork`).
- Compile-time WiFi/MQTT defaults: [`src/Defines.h`](../src/Defines.h) (do not commit secrets into docs).
- Pressure min/max + advanced settings save through `SettingsManager::savePressure` / `saveSetting` (web writes also clamp pressure).

## Networking

### WiFi ([`src/WiFiController.cpp`](../src/WiFiController.cpp))

- Boot: try **STA** with saved SSID/password (20 s timeout). On failure → stay in STA, **no AP fallback**; reconnect every **10 s**.
- **AP mode**: hold encoder button **LOW at boot**. SoftAP SSID `{mqttDeviceName}-WiFi` (or `pump-WiFi`), password `00000000`, IP `192.168.0.1`, channel 6. Auto-off after **5 min**, then switches to STA. LCD shows WiFi icon + **AP** label (and **OTA** when OTA active) and the device **MAC** under MAX.
- STA recovery: sleep off, TX power tweaks, manual reconnect (auto-reconnect disabled).
- Web UI + API start whenever WiFiController inits (AP or STA).
- `NetworkSettings.enableOtaOnNetwork` (default **false**): when true, OTA is allowed in STA; ignored in AP (OTA always on in AP).

### OTA

- **AP:** `ArduinoOTA` always while AP is up.
- **STA:** OTA only when `network.enableOtaOnNetwork` is true and WiFi is connected.
- Started/stopped from `loop()` as link / flag state changes; LCD shows **OTA** under the WiFi icon while active.

### MQTT ([`src/MqttClient.cpp`](../src/MqttClient.cpp))

- Created only when STA is connected (not in AP). Lazy-created later if WiFi connects after boot.
- On connect: publishes device name to `topicTheDeviceIsAlive`; subscribes to `commands/{mqttDeviceName}` and `topicToListenServerWasBorn` (default `homeassistant/status`).
- **Current status:** callback only logs payloads; pump/pressure topic publish not wired yet. Topic name fields exist in settings for future use.

### Web / LittleFS

- Filesystem: **LittleFS** (`board_build.filesystem = littlefs`); assets under [`data/`](../data/) (`index.html`, `css/`, `js/`).
- Flash FS: `pio run -t uploadfs` (in addition to firmware upload).
- Server: [`src/WebServerController.cpp`](../src/WebServerController.cpp) on port **80**
  - `/`, `/index.html` — WiFi (incl. Enable OTA on network), MQTT, and PressureSettings page (API keys `pressure>minAtm` / `maxAtm` / `sensorMaxAtm`; pressures in Atm only)
  - `/settingsApi` — read/write parameters
  - `/log` — in-memory logger dump
  - other paths served from LittleFS

## Key source files

| Area | Files |
|------|--------|
| FSM / control | `src/main.cpp` |
| Display | `src/display.cpp`, `include/display.h` |
| Encoder | `src/encoder.cpp` |
| Pressure ADC | `src/pressure.cpp` |
| Settings / EEPROM | `src/SettingsManager.*`, `src/GlobalSettings.h`, `src/SettingsNavigator.*` |
| WiFi / AP / reconnect | `src/WiFiController.*` |
| HTTP UI | `src/WebServerController.*`, `data/` |
| MQTT | `src/MqttClient.*` |
| Logging | `src/Logger.*`, `src/Defines.h` |
| Pins | `include/pins.h` |

Libs (`platformio.ini`): Adafruit GFX / ST7789 / BusIO, AsyncTCP, ESPAsyncWebServer, PubSubClient.

## Build / flash

```bash
cd /home/dsporynkhin/Projects/cpp/home/presure-controller
pio run
pio run -t upload
pio run -t uploadfs    # LittleFS web assets
pio device monitor     # 115200
```

USB CDC is enabled (`ARDUINO_USB_CDC_ON_BOOT`, `ARDUINO_USB_MODE`).

## Known gaps / WIP

- MQTT: connect + alive + subscribe only; no command handling or state/pressure publishing yet.
- Temporary NDJSON debug logs (session `2ce4f1`) still in `display.cpp` / `main.cpp` — safe to remove after verification.
- Web page title was legacy “Lamp…” — now “Pump WiFi settings”.

## Plan copies in this folder

- `plan-pump-pressure-controller.md` — core pump/UI feature plan (completed; networking is beyond that plan)
- `plan-reset-edit-idle-timer.md` — idle timer on encoder rotation (**done**)
