#include "pressure.h"

Pressure PRESSURE;

float Pressure::voltsToMpa(float voltage) const {
    return (voltage - sensorMinVolts) / ((sensorMaxVolts - sensorMinVolts) / sensorMaxMpa);
}

float Pressure::readBurstVolts() {
    double sum = 0;
    for (int i = 0; i < measurementsCount; ++i) {
        sum += (double)analogReadMilliVolts(pin);
        yield();
        yield();
        yield();
    }
    return static_cast<float>(sum) / static_cast<float>(measurementsCount) / 1000.0f;
}

float Pressure::averageAccumulatedMpa() const {
    if (accumulatedMeasures <= 0) {
        return 0.0f;
    }
    double sum = 0;
    for (int i = 0; i < accumulatedMeasures; ++i) {
        sum += measures[i];
    }
    return static_cast<float>(sum) / static_cast<float>(accumulatedMeasures);
}

void Pressure::pushMeasure(float mpa) {
    if (accumulatedMeasures < samplesCount) {
        measures[accumulatedMeasures++] = mpa;
        return;
    }
    for (int i = 1; i < samplesCount; ++i) {
        measures[i - 1] = measures[i];
    }
    measures[samplesCount - 1] = mpa;
}

bool Pressure::intervalElapsed(unsigned long now) const {
    if (!hasSample) {
        return true;
    }
    return (now - lastMeasureMs) >= static_cast<unsigned long>(measureIntervalMs);
}

void Pressure::takeMeasure(unsigned long now) {
    lastVolts = readBurstVolts();
    pushMeasure(voltsToMpa(lastVolts));
    lastMeasureMs = now;
    hasSample = true;
}

void Pressure::begin(int pin) {
    this->pin = pin;
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    pinMode(this->pin, INPUT);
}

void Pressure::setSensorMaxMpa(float sensorMaxMpa) {
    this->sensorMaxMpa = sensorMaxMpa;
    if (this->sensorMaxMpa < SENSOR_MAX_MIN_MPA) {
        this->sensorMaxMpa = SENSOR_MAX_MIN_MPA;
    }
    if (this->sensorMaxMpa > SENSOR_MAX_MAX_MPA) {
        this->sensorMaxMpa = SENSOR_MAX_MAX_MPA;
    }
}

void Pressure::setSensorVolts(float minVolts, float maxVolts) {
    sensorMinVolts = minVolts;
    sensorMaxVolts = maxVolts;
    if (sensorMinVolts < SENSOR_VOLT_MIN) {
        sensorMinVolts = SENSOR_VOLT_MIN;
    }
    if (sensorMinVolts > SENSOR_VOLT_MAX) {
        sensorMinVolts = SENSOR_VOLT_MAX;
    }
    if (sensorMaxVolts < SENSOR_VOLT_MIN) {
        sensorMaxVolts = SENSOR_VOLT_MIN;
    }
    if (sensorMaxVolts > SENSOR_VOLT_MAX) {
        sensorMaxVolts = SENSOR_VOLT_MAX;
    }
    if (sensorMinVolts >= sensorMaxVolts) {
        sensorMaxVolts = sensorMinVolts + SENSOR_VOLT_STEP;
        if (sensorMaxVolts > SENSOR_VOLT_MAX) {
            sensorMaxVolts = SENSOR_VOLT_MAX;
            sensorMinVolts = SENSOR_VOLT_MAX - SENSOR_VOLT_STEP;
        }
    }
}

void Pressure::setSamplesCount(int count) {
    if (count < SAMPLES_COUNT_MIN) {
        count = SAMPLES_COUNT_MIN;
    }
    if (count > SAMPLES_COUNT_MAX) {
        count = SAMPLES_COUNT_MAX;
    }
    samplesCount = count;
    if (accumulatedMeasures > samplesCount) {
        const int drop = accumulatedMeasures - samplesCount;
        for (int i = 0; i < samplesCount; ++i) {
            measures[i] = measures[i + drop];
        }
        accumulatedMeasures = samplesCount;
    }
}

void Pressure::setMeasureIntervalMs(int intervalMs) {
    if (intervalMs < MEASURE_INTERVAL_MS_MIN) {
        intervalMs = MEASURE_INTERVAL_MS_MIN;
    }
    if (intervalMs > MEASURE_INTERVAL_MS_MAX) {
        intervalMs = MEASURE_INTERVAL_MS_MAX;
    }
    measureIntervalMs = intervalMs;
}

void Pressure::setMeasurementsCount(int count) {
    if (count < MEASUREMENTS_COUNT_MIN) {
        count = MEASUREMENTS_COUNT_MIN;
    }
    if (count > MEASUREMENTS_COUNT_MAX) {
        count = MEASUREMENTS_COUNT_MAX;
    }
    measurementsCount = count;
}

float Pressure::readVolts() {
    const unsigned long now = millis();
    if (intervalElapsed(now)) {
        takeMeasure(now);
    }
    return lastVolts;
}

float Pressure::readMpa() {
    const unsigned long now = millis();
    if (intervalElapsed(now)) {
        takeMeasure(now);
    }
    return averageAccumulatedMpa();
}
