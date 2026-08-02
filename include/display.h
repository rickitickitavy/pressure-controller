#pragma once

#include <Arduino.h>

#include "GlobalSettings.h"

enum class UiMode : uint8_t {
    Run = 0,
    EditMax,
    EditMin,
    Settings,
};

enum class SettingsFocus : uint8_t {
    Leak = 0,
    Weak,
    SensorMax,
    SensorMinVolts,
    SensorMaxVolts,
    Save,
    Cancel,
    Count,
};

struct UiState {
    UiMode mode = UiMode::Run;
    float pressureMpa = 0.1f;
    float minMpa = 0.2f;
    float maxMpa = 0.35f;
    bool pumpOn = false;
    bool pumpControlEnabled = true; // false after MQTT disable or LEAK (red icon)
    bool leakFail = false;          // LEAK latch: red "E" left of pump; cleared by MQTT enable
    bool wifiIcon = false; // AP active or STA connected
    bool apMode = false;   // show "AP" under WiFi icon + MAC under MAX
    bool otaActive = false; // show "OTA" under WiFi icon (below AP when AP)
    int8_t wifiRssiPercent = -1; // 0–100 when STA/AP connected; -1 = hidden
    char macAddress[18] = {}; // "AA:BB:CC:DD:EE:FF"; used in AP mode
    PressureSettings draft{};
    SettingsFocus focus = SettingsFocus::Leak;
};

namespace Display {
    void begin(bool rotate180);

    void render(const UiState &state);

    void invalidateWifi();
} // namespace Display
