#pragma once

#include <Arduino.h>

namespace Pressure {

    void begin(int pin);

    void setSensorMaxMpa(float sensorMaxMpa);

    void setSensorVolts(float minVolts, float maxVolts);

    float readMpa(); // averaged reading in MPa
} // namespace Pressure
