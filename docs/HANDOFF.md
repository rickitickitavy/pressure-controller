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

- Display: **240×320**, base rotation **180°** (`setRotation(2)`), no backlight pin. Web setting `display>rotate180` (default **OFF**): when **ON**, adds another 180° → `setRotation(0)`. Applied in `Display::begin` at boot.
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
4. **Pump** — icon (not ON/OFF text); **green** when ON, **gray** when OFF, **red** when pump control is disabled (MQTT `disable` or LEAK timeout)  
5. **WiFi** — blue icon to the right of the pump when STA connected or AP active. Stack under the icon (FreeSansBold **9pt**, blue, centered): **signal %** (STA or AP), then **AP** (AP mode), then **OTA** (when OTA active). Percent is hidden when not connected.  
6. **MIN** — white on dark green (edit: black on yellow); FreeSansBold 24pt  

`UiState.macAddress` is filled from `WiFi.macAddress()` in `main.cpp` only while `apMode` is true.  
`UiState.wifiRssiPercent` is the 2 s average of `WiFi.RSSI()` mapped to 0–100% (`≤ -100 → 0`, `≥ -50 → 100`, else `2*(rssi+100)`); updated at most once per **2 s** in `main.cpp`.

Partial bar redraws only (no full-screen clear in normal updates).

## Modes

| Mode | How |
|------|-----|
| Run | Default control |
| Edit MAX / MIN | Short button: Run → EditMax → EditMin → Run; rotate ±0.001 MPa; idle **5 s** (button or rotate resets timer) |
| SETTINGS | Hold button **≥ 3 s** |

### Fail-safe / pump limits

When the pump turns **ON**:

1. **LEAK** (reach-min): if pressure has not reached **min** within `leakDetectSec` → same as MQTT **`disable`**: pump OFF, auto control blocked (`gPumpControlEnabled = false`), pump icon **red**. UI stays interactive. Restore with MQTT **`enable`**.
2. **WEAK** (max continuous run): if pump stays ON longer than `pumpWeakSec` → **pump OFF only** (control stays enabled). May turn ON again on the next loop if pressure is still below min (fresh weak timer).

### SETTINGS screen (LCD)

Rows: **LEAK**, **WEAK**, **SENS**, **VMIN**, **VMAX**, **SAMP**, **INTV**, **MCNT**, **WIFI**, **SAVE**, **CANCEL**

- Rotate: move focus (wrap); in edit mode change the focused value  
- Short click: enter/exit field edit (yellow = focus, cyan = editing); on **SAVE** / **CANCEL** activate  
- Hold **≥ 3 s** from Run/Edit: enter Settings  
- Idle **10 s**: CANCEL  

Draft values apply live on the screen; EEPROM write only on **SAVE**.

| Setting | Range | Step | Default |
|---------|-------|------|---------|
| LEAK (reach min after pump ON) | 5–40 s | 1 s | 10 |
| WEAK (max pump ON; then OFF, control stays enabled) | 40–600 s | 1 s | 180 |
| SENS (sensor full-scale) | 2.0–50.0 Atm | 0.1 Atm | 5.0 |

## Persistence & settings model

- Stored in **EEPROM** (4096 bytes) via [`src/SettingsManager.cpp`](../src/SettingsManager.cpp) — **not** `Preferences`/NVS.
- Schema: [`src/GlobalSettings.h`](../src/GlobalSettings.h) — `NetworkSettings` (incl. `enableOtaOnNetwork`), MQTT fields (incl. `pressureUpdateDiffAtm`, `pressurePubMinIntSec`, `pumpStatePubMinIntSec`, auth username/password, `mqttEnabled`), `PressureSettings` (`minMpa` / `maxMpa` / `leakDetectSec` / `pumpWeakSec` / `sensorMaxMpa` as `float`/`int`), `displayRotate180` (bool), version/marker (`GLOBAL_CURRENT_SETTINGS_VERSION = 5`). No brightness field.
- First boot (invalid marker): single `SettingsManager::applyDefaults()` fills all defaults, then save + restart.
- Version mismatch: upgrade branch kept (log + stamp version + save) but **no field migrations** yet.
- Web/API parameter names: [`src/SettingsNavigator.cpp`](../src/SettingsNavigator.cpp) + `ParamDescriptor` (incl. `BOOLEAN`, pressure `*`, `network>enableOtaOnNetwork`, `display>rotate180`).
- Compile-time WiFi/MQTT defaults: [`src/Defines.h`](../src/Defines.h) (do not commit secrets into docs).
- Pressure min/max + advanced settings save through `SettingsManager::savePressure` / `saveSetting` (web writes also clamp pressure).

## Networking

### WiFi ([`src/WiFiController.cpp`](../src/WiFiController.cpp))

- Boot: try **STA** with saved SSID/password (20 s timeout). On failure → stay in STA, **no AP fallback**; reconnect every **10 s**.
- **AP mode**: hold encoder button **LOW at boot**. SoftAP SSID `{mqttDeviceName}-WiFi` (or `pump-WiFi`), password `00000000`, IP `192.168.0.1`, channel 6. Auto-off after **5 min**, then switches to STA. LCD shows WiFi icon + **signal %** + **AP** label (and **OTA** when OTA active) and the device **MAC** under MAX.
- STA recovery: sleep off, TX power tweaks, manual reconnect (auto-reconnect disabled).
- Web UI + API start whenever WiFiController inits (AP or STA).
- `NetworkSettings.enableOtaOnNetwork` (default **false**): when true, **ArduinoOTA** is allowed in STA; ignored in AP (ArduinoOTA always on in AP). Does **not** gate HTTP firmware/data upload from the web UI.

### OTA

#### ArduinoOTA (PlatformIO / `espota`)

- **AP:** always while AP is up.
- **STA:** only when `network.enableOtaOnNetwork` is true and WiFi is connected.
- Started/stopped from `loop()` as link / flag state changes; LCD shows **OTA** under the WiFi icon (below **signal %** / **AP**) while active.
- Skipped while an HTTP upload is in progress (`WebServerController::isHttpUploadInProgress()`).

#### HTTP upload (web UI Update tab)

- Always allowed when the web server is reachable (AP or STA connected); **ignores** `enableOtaOnNetwork`.
- `POST /update/code` — app firmware (`U_FLASH`); upload PlatformIO `firmware.bin`.
- `POST /update/data` — LittleFS image (`U_SPIFFS`); upload `littlefs.bin` from `pio run -t buildfs` / `uploadfs`.
- On success the device reboots.

### MQTT ([`src/MqttClient.cpp`](../src/MqttClient.cpp))

- Created only when STA is connected (not in AP). Lazy-created later if WiFi connects after boot.
- On connect (and on `topicToListenServerWasBorn` = `online`): publish device name to `topicTheDeviceIsAlive`; publish retained pump `"ON"`/`"OFF"` to `topicThePumpState/{mqttDeviceName}`; publish pressure (Atm, 2 decimals) to `topicPressureValue/{mqttDeviceName}`; subscribe to `topicToListenCommands/{mqttDeviceName}` and `topicToListenServerWasBorn` (default `homeassistant/status`).
- Ongoing pump publish: on actual pump state change via `notifyPumpState` from `setPump` (retained), rate-limited by `mqtt>pumpStatePubMinIntSec` (default **10**, clamp **5–120**). Changes during cooldown are held and published with the latest state once the interval elapses. Ongoing pressure publish: only when `|Δatm| > mqtt>pressureUpdateDiffAtm` (default **0.05**, clamp **0.01–0.5**; Web UI step **0.01**) and at least `mqtt>pressurePubMinIntSec` (default **10**, clamp **5–120**) has passed since the last pressure publish. No pressure heartbeat.
- Commands on `commands/{device}`: `on` / `off` (case-insensitive). `off` forces pump OFF, blocks auto ON, and shows pump icon **red**; `on` restores auto control (green/gray icon). **LEAK** timeout uses the same disable path. Web Status tab enable toggle uses the same `setPumpControlEnabled` path and publishes retained state on the enabled topic when MQTT is up.
- Settings `pressureUpdateDiffAtm`, `pressurePubMinIntSec`, `pumpStatePubMinIntSec`, and `displayRotate180` are appended on `GlobalSettings` (no settings-version bump); out-of-range EEPROM values reset to defaults on load (`displayRotate180` default **false** / OFF).

### Web / LittleFS

- Filesystem: **LittleFS** (`board_build.filesystem = littlefs`); assets under [`data/`](../data/) (`index.html`, `css/all.css`).
- Flash FS: `pio run -t uploadfs` (in addition to firmware upload).
- Firmware label: `FIRMWARE_VERSION` in [`src/Defines.h`](../src/Defines.h) (exposed on Status / `/statusApi`).
- Server: [`src/WebServerController.cpp`](../src/WebServerController.cpp) on port **80**
  - `/`, `/index.html` — left-nav UI with three sections:
    - **Status** — live pressure, pump ON/OFF, enable toggle, IP, RSSI, MQTT connected, firmware version; polls `GET /statusApi` every **1 s** only while Status is visible
    - **Settings** — existing horizontal sub-tabs (WiFi incl. Enable OTA on network / Display `display>rotate180` / MQTT / Pressure / Sensor); pressures in Atm (`pressure>minAtm` / `maxAtm` / `sensorMaxAtm`)
    - **Update** — upload Code (`firmware.bin`) and Data (`littlefs.bin`)
  - `/settingsApi` — read/write parameters
  - `/statusApi` — `GET` JSON (`pressureAtm`, `pumpOn`, `pumpControlEnabled`, `firmwareVersion`, `ip`, `rssi`, `mqttConnected`); `POST ?operation=write&enabled=0|1` toggles pump control (same handler as MQTT `on`/`off`)
  - `/sensorVoltage` — live sensor volts (Settings → Sensor sub-tab)
  - `/update/code`, `/update/data` — HTTP OTA (see above)
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

Flash: **4 MB**. Partition scheme: **`min_spiffs`** (`board_build.partitions` in [`platformio.ini`](../platformio.ini)) — dual OTA app slots **0x1E0000** (~1.875 MB), LittleFS **0x20000** (128 KB). Default `0x140000` app slots are too small for current firmware.

After changing the partition table, re-flash firmware **and** `uploadfs` (filesystem is wiped/resized).

```bash
cd /home/dsporynkhin/Projects/cpp/home/presure-controller
pio run
pio run -t upload
pio run -t uploadfs    # LittleFS web assets
pio device monitor     # 115200
```

USB CDC is enabled (`ARDUINO_USB_CDC_ON_BOOT`, `ARDUINO_USB_MODE`).

## Plan copies in this folder

- `plan-pump-pressure-controller.md` — core pump/UI feature plan (completed; networking is beyond that plan)
- `plan-reset-edit-idle-timer.md` — idle timer on encoder rotation (**done**)
