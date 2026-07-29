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
    PressureSettings draft{};
    SettingsFocus focus = SettingsFocus::Leak;
};

namespace Display {
    void begin();

    void render(const UiState &state);
} // namespace Display
