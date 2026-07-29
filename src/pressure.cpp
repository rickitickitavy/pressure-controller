#include "pressure.h"

#include "GlobalSettings.h"

namespace {
    int gPin = -1;
    float gSensorMaxMpa = PRESSURE_SENSOR_MAX_DEFAULT_MPA;
    constexpr int kSamples = 8;
} // namespace

namespace Pressure {
    void begin(int pin) {
        gPin = pin;
        analogReadResolution(12);
        analogSetAttenuation(ADC_11db);
        pinMode(gPin, INPUT);
    }

    void setSensorMaxMpa(float sensorMaxMpa) {
        gSensorMaxMpa = sensorMaxMpa;
        if (gSensorMaxMpa < SENSOR_MAX_MIN_MPA) {
            gSensorMaxMpa = SENSOR_MAX_MIN_MPA;
        }
        if (gSensorMaxMpa > SENSOR_MAX_MAX_MPA) {
            gSensorMaxMpa = SENSOR_MAX_MAX_MPA;
        }
    }

    float readMpa() {
        uint32_t sum = 0;
        for (int i = 0; i < kSamples; ++i) {
            sum += analogRead(gPin);
        }
        const float adc = static_cast<float>(sum) / static_cast<float>(kSamples);
        // ADC 0 -> vacuum (0 MPa / 0 psi), ADC 4095 -> sensorMax
        float mpa = (adc / 4095.0f) * gSensorMaxMpa - 0.1f;
        // #endregion
        return mpa;
    }
} // namespace Pressure
