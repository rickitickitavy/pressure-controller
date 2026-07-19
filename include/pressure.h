#pragma once

#include <Arduino.h>

namespace Pressure {
    void begin(int pin);

    void setSensorMaxMpa(float sensorMaxMpa);

    float readMpa(); // averaged reading in MPa
} // namespace Pressure
