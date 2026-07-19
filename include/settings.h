#pragma once

#include <Arduino.h>

struct PressureSettings {
    float minMpa = 0.200f;
    float maxMpa = 0.350f;
    uint16_t leakDetectSec = 10; // time to reach min after pump ON
    uint16_t pumpWeakSec = 180; // max continuous pump ON
    float sensorMaxMpa = 0.500f; // ADC full-scale pressure
};

namespace Settings {
    void begin();

    PressureSettings load();

    void save(const PressureSettings &s);

    void clampPair(float &minMpa, float &maxMpa, float sensorMaxMpa);

    void clampAdvanced(PressureSettings &s);
} // namespace Settings

constexpr float PRESSURE_ATM_MPA = 0.100f; // 1 Atm = 0.1 MPa (display conversion only)
constexpr float PRESSURE_SENSOR_MAX_DEFAULT_MPA = 0.500f;
constexpr float PRESSURE_STEP_MPA = 0.001f;
constexpr float PRESSURE_MIN_GAP_MPA = 0.002f;
constexpr float SENSOR_MAX_STEP_MPA = 0.010f; // 0.1 atm
constexpr float SENSOR_MAX_MIN_MPA = 0.200f; // 2.0 atm
constexpr float SENSOR_MAX_MAX_MPA = 5.000f; // 50.0 atm

constexpr uint16_t LEAK_SEC_MIN = 5;
constexpr uint16_t LEAK_SEC_MAX = 40;
constexpr uint16_t WEAK_SEC_MIN = 40;
constexpr uint16_t WEAK_SEC_MAX = 600;
