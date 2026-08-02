#include "pressure.h"

#include "GlobalSettings.h"
#include "Logger.h"

namespace {
    int gPin = -1;
    float gSensorMaxMpa = PRESSURE_SENSOR_MAX_DEFAULT_MPA;
    float gSensorMinVolts = SENSOR_VOLT_MIN_DEFAULT;
    float gSensorMaxVolts = SENSOR_VOLT_MAX_DEFAULT;
    constexpr int kSamples = 16;

    float readAveragedVolts() {
        double sum = 0;
        for (int i = 0; i < kSamples; ++i) {
            sum += (double)analogReadMilliVolts(gPin);
            yield();
            yield();
            yield();
            delay(2);
        }
        const float adc = static_cast<float>(sum) / static_cast<float>(kSamples);
        return adc / 1000.0f;
    }
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

    void setSensorVolts(float minVolts, float maxVolts) {
        gSensorMinVolts = minVolts;
        gSensorMaxVolts = maxVolts;
        if (gSensorMinVolts < SENSOR_VOLT_MIN) {
            gSensorMinVolts = SENSOR_VOLT_MIN;
        }
        if (gSensorMinVolts > SENSOR_VOLT_MAX) {
            gSensorMinVolts = SENSOR_VOLT_MAX;
        }
        if (gSensorMaxVolts < SENSOR_VOLT_MIN) {
            gSensorMaxVolts = SENSOR_VOLT_MIN;
        }
        if (gSensorMaxVolts > SENSOR_VOLT_MAX) {
            gSensorMaxVolts = SENSOR_VOLT_MAX;
        }
        if (gSensorMinVolts >= gSensorMaxVolts) {
            gSensorMaxVolts = gSensorMinVolts + SENSOR_VOLT_STEP;
            if (gSensorMaxVolts > SENSOR_VOLT_MAX) {
                gSensorMaxVolts = SENSOR_VOLT_MAX;
                gSensorMinVolts = SENSOR_VOLT_MAX - SENSOR_VOLT_STEP;
            }
        }
    }

    float readVolts() {
        return readAveragedVolts();
    }

    float readMpa() {
        float voltage = readAveragedVolts();
        float mpa = (voltage - gSensorMinVolts) / ((gSensorMaxVolts - gSensorMinVolts) / gSensorMaxMpa);

        // LOGGER.info("voltage = " + String(voltage) + "   pressure = " + String(mpa));
        return mpa;
    }
} // namespace Pressure
