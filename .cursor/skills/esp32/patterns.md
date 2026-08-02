# ESP32 patterns (pressure-controller)

Project-derived detail only. Read when implementing persistence, OTA, or SPI display init.

## SPI MISO on ESP32-C3

ST7789 may not need MISO, but the ESP32-C3 SPI HAL rejects `MISO = -1`. Assign a real unused GPIO (this project: pin 5) and document it in `pins.h`.

## Settings persistence

- Store settings in **EEPROM** (sized as in the project, e.g. 4096 bytes) via a settings manager — not `Preferences`/NVS.
- Use a versioned global settings struct with a marker; first boot applies defaults then save/restart.
- On version mismatch, follow the existing upgrade path; do not invent Preferences migration.

## OTA gating

| Mode | ArduinoOTA |
|------|------------|
| AP | Always on while AP is up |
| STA | Only if `network.enableOtaOnNetwork` and WiFi connected |

Start/stop OTA from `loop()` as link/flag state changes. Keep `ArduinoOTA.handle()` while enabled.

## LittleFS web assets

- Mount LittleFS early in `setup`.
- Serve static files from `data/` via the project web server.
- After changing `data/`, run `pio run -t uploadfs` in addition to firmware upload.

## Callback wiring

Prefer registering handlers from `main.cpp` (or another owner) instead of a fake singleton inside the class:

```cpp
// GOOD — ownership outside the class
mqtt = new MqttClient(settings);
mqtt->setPumpControlHandler(onMqttPumpControl);
```

```cpp
// BAD — instance pointer only for a library callback
static MqttClient *instance;
instance = this;
```
