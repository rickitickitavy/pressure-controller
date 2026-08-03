#pragma once

#include <Arduino.h>

#include "GlobalSettings.h"

class Pressure {
public:
    float measures[SAMPLES_COUNT_MAX] = {};
    int accumulatedMeasures = 0;

    void begin(int pin);

    void setSensorMaxMpa(float sensorMaxMpa);

    void setSensorVolts(float minVolts, float maxVolts);

    void setSamplesCount(int count);

    void setMeasureIntervalMs(int intervalMs);

    void setMeasurementsCount(int count);

    float readVolts(); // last real-measure sensor voltage
    float readMpa(); // average of accumulated measures in MPa

private:
    int pin = -1;
    float sensorMaxMpa = PRESSURE_SENSOR_MAX_DEFAULT_MPA;
    float sensorMinVolts = SENSOR_VOLT_MIN_DEFAULT;
    float sensorMaxVolts = SENSOR_VOLT_MAX_DEFAULT;
    int samplesCount = SAMPLES_COUNT_DEFAULT;
    int measureIntervalMs = MEASURE_INTERVAL_MS_DEFAULT;
    int measurementsCount = MEASUREMENTS_COUNT_DEFAULT;

    unsigned long lastMeasureMs = 0;
    bool hasSample = false;
    float lastVolts = 0.0f;

    float voltsToMpa(float voltage) const;
    float readBurstVolts();
    float averageAccumulatedMpa() const;
    void pushMeasure(float mpa);
    bool intervalElapsed(unsigned long now) const;
    void takeMeasure(unsigned long now);
};

extern Pressure PRESSURE;
