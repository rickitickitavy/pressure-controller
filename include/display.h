#pragma once

#include <Arduino.h>

#include "GlobalSettings.h"

enum class UiMode : uint8_t {
    Run = 0,
    EditMax,
    EditMin,
    Fail,
    Settings,
};

enum class SettingsFocus : uint8_t {
    Leak = 0,
    Weak,
    SensorMax,
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
    bool wifiIcon = false; // AP active or STA connected
    bool apMode = false;   // show "AP" under WiFi icon + MAC under MAX
    char macAddress[18] = {}; // "AA:BB:CC:DD:EE:FF"; used in AP mode
    PressureSettings draft{};
    SettingsFocus focus = SettingsFocus::Leak;
};

namespace Display {
    void begin();

    void render(const UiState &state);

    void invalidateWifi();
} // namespace Display
