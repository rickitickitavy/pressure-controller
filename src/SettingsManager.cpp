#include "SettingsNavigator.h"//
#include "Logger.h"
// Created by dsporykhin on 23.04.20.
//

#include <EEPROM.h>
#include <math.h>
#include <string.h>

SettingsManager::SettingsManager(){

    EEPROM.begin(4096);
    LOGGER.info("Load settings...");
    readSettings();
    navigator = new SettingsNavigator(this);

    if ((settings.initMarker[0] != GLOBAL_SETTINGS_MARKER_0)
        || (settings.initMarker[1] != GLOBAL_SETTINGS_MARKER_1)
        || (settings.initMarker[2] != GLOBAL_SETTINGS_MARKER_2)
        || (settings.initMarker[3] != GLOBAL_SETTINGS_MARKER_3)) {
        LOGGER.error("Settings has never been initialized. Initializing by default config...");
        applyDefaults();
        saveSetting(true);
        LOGGER.warning("Settings has never been initialized");
    } else {

        LOGGER.info("Settings loaded. Version " + String(settings.version));
        if (settings.version != GLOBAL_CURRENT_SETTINGS_VERSION) {

            LOGGER.warning("Upgrade settings to version " + String(GLOBAL_CURRENT_SETTINGS_VERSION));

            // v1 -> v2: PressureSettings gained leakDetectEnabled (shifts trailing fields).
            settings.pressure.leakDetectEnabled = true;
            // v2 -> v3: PressureSettings gained sensorMinVolts / sensorMaxVolts.
            settings.pressure.sensorMinVolts = SENSOR_VOLT_MIN_DEFAULT;
            settings.pressure.sensorMaxVolts = SENSOR_VOLT_MAX_DEFAULT;
            // v3 -> v4: PressureSettings gained samplesCount / measureIntervalMs / measurementsCount.
            settings.pressure.samplesCount = SAMPLES_COUNT_DEFAULT;
            settings.pressure.measureIntervalMs = MEASURE_INTERVAL_MS_DEFAULT;
            settings.pressure.measurementsCount = MEASUREMENTS_COUNT_DEFAULT;
            // v4 -> v5: NetworkSettings gained wifiEnabled.
            settings.network.wifiEnabled = false;

            settings.version = GLOBAL_CURRENT_SETTINGS_VERSION;
            LOGGER.warning(" Upgrade settings finished");
            saveSetting(false);
        }

        // Sanitize out-of-range EEPROM values (no schema version bump for these fields).
        clampAdvanced(settings.pressure);
        clampPair(settings.pressure.minMpa, settings.pressure.maxMpa, settings.pressure.sensorMaxMpa);
        clampPressureUpdateDiff(settings.pressureUpdateDiffAtm);
        clampMqttPublishMinInterval(settings.pressurePubMinIntSec);
        clampMqttPublishMinInterval(settings.pumpStatePubMinIntSec);
        clampMqttClientTimeout(settings.mqttClientTimeoutMs);
        {
            uint8_t raw = 0;
            memcpy(&raw, &settings.displayRotate180, sizeof(raw));
            if (raw > 1) {
                settings.displayRotate180 = false;
            }
        }
        {
            uint8_t raw = 0;
            memcpy(&raw, &settings.mqttEnabled, sizeof(raw));
            if (raw > 1) {
                settings.mqttEnabled = false;
            }
        }
    }


    logSettings();
};
//--------------------------------------------------------------------

void SettingsManager::applyDefaults() {
    settings.initMarker[0] = GLOBAL_SETTINGS_MARKER_0;
    settings.initMarker[1] = GLOBAL_SETTINGS_MARKER_1;
    settings.initMarker[2] = GLOBAL_SETTINGS_MARKER_2;
    settings.initMarker[3] = GLOBAL_SETTINGS_MARKER_3;
    settings.version = GLOBAL_CURRENT_SETTINGS_VERSION;

    resetWiFi();
    settings.network.enableOtaOnNetwork = false;
    settings.network.wifiEnabled = false;

    settings.mqttPort = 1883;
    String(DEFAULT_MQTT_SERVER).toCharArray(settings.mqttServer, sizeof(settings.mqttServer));
    settings.mqttReconnectIntervalMs = 1000;

    String(DEFAULT_TOPIC_THE_DEVICE_IS_ALIVE).toCharArray(
            settings.topicTheDeviceIsAlive, sizeof(settings.topicTheDeviceIsAlive));

    String(DEFAULT_TOPIC_THE_PUMP_STATE).toCharArray(
            settings.topicThePumpState, sizeof(settings.topicThePumpState));

    String(DEFAULT_TOPIC_PRESSURE_VALUE).toCharArray(
            settings.topicPressureValue, sizeof(settings.topicPressureValue));

    String(DEFAULT_TOPIC_TO_LISTEN_COMMANDS).toCharArray(
            settings.topicToListenCommands, sizeof(settings.topicToListenCommands));

    String(DEFAULT_TOPIC_TO_LISTEN_SERVER_WAS_BORN).toCharArray(
            settings.topicToListenServerWasBorn, sizeof(settings.topicToListenServerWasBorn));

    String(DEFAULT_TOPIC_IS_THE_DEVICE_ENABLED).toCharArray(
            settings.topicIsTheDeviceEnabled, sizeof(settings.topicIsTheDeviceEnabled));

    String("device").toCharArray(settings.mqttDeviceName, sizeof(settings.mqttDeviceName));

    settings.pressureUpdateDiffAtm = DEFAULT_MQTT_PRESSURE_UPDATE_DIFF_ATM;
    settings.pressurePubMinIntSec = MQTT_PUBLISH_MIN_INTERVAL_SEC_DEFAULT;
    settings.pumpStatePubMinIntSec = MQTT_PUBLISH_MIN_INTERVAL_SEC_DEFAULT;
    settings.displayRotate180 = false;
    settings.mqttEnabled = true;
    settings.mqttClientTimeoutMs = MQTT_CLIENT_TIMEOUT_MS_DEFAULT;
    settings.mqttUsername[0] = '\0';
    settings.mqttPassword[0] = '\0';

    settings.pressure = PressureSettings{};
    clampAdvanced(settings.pressure);
    clampPair(settings.pressure.minMpa, settings.pressure.maxMpa, settings.pressure.sensorMaxMpa);
}
//--------------------------------------------------------------------

void SettingsManager::clampPressureUpdateDiff(float &diffAtm) {
    if (!(diffAtm >= MQTT_PRESSURE_UPDATE_DIFF_MIN_ATM &&
          diffAtm <= MQTT_PRESSURE_UPDATE_DIFF_MAX_ATM)) {
        diffAtm = MQTT_PRESSURE_UPDATE_DIFF_DEFAULT_ATM;
    }
}
//--------------------------------------------------------------------

void SettingsManager::clampMqttPublishMinInterval(int &intervalSec) {
    if (intervalSec < MQTT_PUBLISH_MIN_INTERVAL_SEC_MIN ||
        intervalSec > MQTT_PUBLISH_MIN_INTERVAL_SEC_MAX) {
        intervalSec = MQTT_PUBLISH_MIN_INTERVAL_SEC_DEFAULT;
    }
}
//--------------------------------------------------------------------

void SettingsManager::clampMqttClientTimeout(int &timeoutMs) {
    if (timeoutMs < MQTT_CLIENT_TIMEOUT_MS_MIN ||
        timeoutMs > MQTT_CLIENT_TIMEOUT_MS_MAX) {
        timeoutMs = MQTT_CLIENT_TIMEOUT_MS_DEFAULT;
    }
}
//--------------------------------------------------------------------

void SettingsManager::clampAdvanced(PressureSettings &s) {
    if (s.leakDetectSec < LEAK_SEC_MIN) {
        s.leakDetectSec = LEAK_SEC_MIN;
    }
    if (s.leakDetectSec > LEAK_SEC_MAX) {
        s.leakDetectSec = LEAK_SEC_MAX;
    }
    if (s.pumpWeakSec < WEAK_SEC_MIN) {
        s.pumpWeakSec = WEAK_SEC_MIN;
    }
    if (s.pumpWeakSec > WEAK_SEC_MAX) {
        s.pumpWeakSec = WEAK_SEC_MAX;
    }
    if (s.sensorMaxMpa < SENSOR_MAX_MIN_MPA) {
        s.sensorMaxMpa = SENSOR_MAX_MIN_MPA;
    }
    if (s.sensorMaxMpa > SENSOR_MAX_MAX_MPA) {
        s.sensorMaxMpa = SENSOR_MAX_MAX_MPA;
    }
    // Snap to 0.1 atm steps
    const float steps = roundf(s.sensorMaxMpa / SENSOR_MAX_STEP_MPA);
    s.sensorMaxMpa = steps * SENSOR_MAX_STEP_MPA;
    if (s.sensorMaxMpa < SENSOR_MAX_MIN_MPA) {
        s.sensorMaxMpa = SENSOR_MAX_MIN_MPA;
    }
    if (s.sensorMaxMpa > SENSOR_MAX_MAX_MPA) {
        s.sensorMaxMpa = SENSOR_MAX_MAX_MPA;
    }

    auto clampVolt = [](float v) {
        if (v < SENSOR_VOLT_MIN) {
            v = SENSOR_VOLT_MIN;
        }
        if (v > SENSOR_VOLT_MAX) {
            v = SENSOR_VOLT_MAX;
        }
        const float snapped = roundf(v / SENSOR_VOLT_STEP) * SENSOR_VOLT_STEP;
        if (snapped < SENSOR_VOLT_MIN) {
            return SENSOR_VOLT_MIN;
        }
        if (snapped > SENSOR_VOLT_MAX) {
            return SENSOR_VOLT_MAX;
        }
        return snapped;
    };
    s.sensorMinVolts = clampVolt(s.sensorMinVolts);
    s.sensorMaxVolts = clampVolt(s.sensorMaxVolts);
    if (s.sensorMinVolts >= s.sensorMaxVolts) {
        if (s.sensorMaxVolts - SENSOR_VOLT_STEP >= SENSOR_VOLT_MIN) {
            s.sensorMinVolts = s.sensorMaxVolts - SENSOR_VOLT_STEP;
        } else {
            s.sensorMaxVolts = s.sensorMinVolts + SENSOR_VOLT_STEP;
            if (s.sensorMaxVolts > SENSOR_VOLT_MAX) {
                s.sensorMaxVolts = SENSOR_VOLT_MAX;
                s.sensorMinVolts = SENSOR_VOLT_MAX - SENSOR_VOLT_STEP;
            }
        }
    }

    if (s.samplesCount < SAMPLES_COUNT_MIN || s.samplesCount > SAMPLES_COUNT_MAX) {
        s.samplesCount = SAMPLES_COUNT_DEFAULT;
    }
    if (s.measureIntervalMs < MEASURE_INTERVAL_MS_MIN ||
        s.measureIntervalMs > MEASURE_INTERVAL_MS_MAX) {
        s.measureIntervalMs = MEASURE_INTERVAL_MS_DEFAULT;
    }
    if (s.measurementsCount < MEASUREMENTS_COUNT_MIN ||
        s.measurementsCount > MEASUREMENTS_COUNT_MAX) {
        s.measurementsCount = MEASUREMENTS_COUNT_DEFAULT;
    }
}
//--------------------------------------------------------------------

void SettingsManager::clampPair(float &minMpa, float &maxMpa, float sensorMaxMpa) {
    if (sensorMaxMpa < SENSOR_MAX_MIN_MPA) {
        sensorMaxMpa = SENSOR_MAX_MIN_MPA;
    }
    if (minMpa < 0.0f) {
        minMpa = 0.0f;
    }
    if (maxMpa > sensorMaxMpa) {
        maxMpa = sensorMaxMpa;
    }
    if (minMpa > sensorMaxMpa - PRESSURE_MIN_GAP_MPA) {
        minMpa = sensorMaxMpa - PRESSURE_MIN_GAP_MPA;
    }
    if (maxMpa < PRESSURE_MIN_GAP_MPA) {
        maxMpa = PRESSURE_MIN_GAP_MPA;
    }
    if (minMpa >= maxMpa) {
        if (maxMpa - PRESSURE_MIN_GAP_MPA >= 0.0f) {
            minMpa = maxMpa - PRESSURE_MIN_GAP_MPA;
        } else {
            maxMpa = minMpa + PRESSURE_MIN_GAP_MPA;
            if (maxMpa > sensorMaxMpa) {
                maxMpa = sensorMaxMpa;
                minMpa = maxMpa - PRESSURE_MIN_GAP_MPA;
            }
        }
    }
}
//--------------------------------------------------------------------

SettingsNavigator* SettingsManager::getNavigator(){
    return navigator;
}
//--------------------------------------------------------------------

void SettingsManager::readSettings(GlobalSettings* settings) {
    char *bufPtr = (char *) settings;
    LOGGER.info("Loading " + String((int)sizeof(GlobalSettings)) + " bytes");
    for (int i = 0; i < sizeof(GlobalSettings); i++) {
        bufPtr[i] = EEPROM.read(i);
    }
    LOGGER.info("Settings read");
}
//--------------------------------------------------------------------

void SettingsManager::readSettings() {
    readSettings(&settings);
}
//--------------------------------------------------------------------

void SettingsManager::saveSetting(bool restart) {
    saveSetting(&settings, restart);
}
//--------------------------------------------------------------------

void SettingsManager::saveSetting(GlobalSettings* settingsToSave, bool restart) {
    LOGGER.info(" Saving settings ...");

    char *dataPtr = (char *) settingsToSave;
    for (int addr = 0; addr < sizeof(GlobalSettings); addr++) {
        EEPROM.write(addr, dataPtr[addr]);
    }
    EEPROM.commit();
    LOGGER.info("saved");

    // Never ESP.restart() here — AsyncWebServer handlers must return first.
    // Main loop (or setup) calls handlePendingRestart().
    if (restart) {
        pendingRestart = true;
        restartRequestedMs = millis();
        LOGGER.warning("Restart scheduled");
    }
}
//--------------------------------------------------------------------

bool SettingsManager::handlePendingRestart(unsigned long delayMs) {
    if (!pendingRestart) {
        return false;
    }
    if ((millis() - restartRequestedMs) < delayMs) {
        return false;
    }
    LOGGER.warning("RESTARTING...");
    ESP.restart();
    return true;
}
//--------------------------------------------------------------------

void SettingsManager::savePressure(const PressureSettings& pressure) {
    settings.pressure = pressure;
    clampAdvanced(settings.pressure);
    clampPair(settings.pressure.minMpa, settings.pressure.maxMpa, settings.pressure.sensorMaxMpa);
    saveSetting(false);
}
//--------------------------------------------------------------------

void SettingsManager::resetWiFi() {
    String temp = WIFI_DEFAULT_SID;
    temp.toCharArray(&settings.network.ssid[0], sizeof(settings.network.ssid));

    temp = WIFI_DEFAULT_PASSWORD;
    temp.toCharArray(&settings.network.password[0], sizeof(settings.network.password));

    temp = WIFI_DEFAULT_HOST_NAME;
    temp.toCharArray(&settings.network.hostName[0], sizeof(settings.network.hostName));
}
//--------------------------------------------------------------------

GlobalSettings* SettingsManager::getSettings(){
    return &settings;
}
//--------------------------------------------------------------------

void SettingsManager::logSettings() {
    LOGGER.info("----- SETTINGS ----");
    LOGGER.info("   wifw:");
    LOGGER.info("      SSID: " + String(settings.network.ssid));
    LOGGER.info("      password: " + String(settings.network.password));
    LOGGER.info("      host: " + String(settings.network.hostName));
    LOGGER.info("      enableOtaOnNetwork: " + String(settings.network.enableOtaOnNetwork ? "true" : "false"));
    LOGGER.info("      wifiEnabled: " + String(settings.network.wifiEnabled ? "true" : "false"));
    LOGGER.info("   mqtt:");
    LOGGER.info("      enabled: " + String(settings.mqttEnabled ? "true" : "false"));
    LOGGER.info("      server: " + String(settings.mqttServer));
    LOGGER.info("      port: " + String(settings.mqttPort));
    LOGGER.info("      reconIntervalMs: " + String(settings.mqttReconnectIntervalMs));
    LOGGER.info("      clientTimeoutMs: " + String(settings.mqttClientTimeoutMs));
    LOGGER.info("      device name: " + String(settings.mqttDeviceName));
    if (settings.mqttUsername[0] != '\0') {
        LOGGER.info("      username: " + String(settings.mqttUsername));
        LOGGER.info("      password: (set)");
    }
    LOGGER.info("      The name of the topic to report that the device is alive:   " + String(settings.topicTheDeviceIsAlive));
    LOGGER.info("      The name of topic to report the pump state:                 " + String(settings.topicThePumpState) + "/" + String(settings.mqttDeviceName));
    LOGGER.info("      The name of topic to report the pressure value:             " + String(settings.topicPressureValue) + "/" + String(settings.mqttDeviceName));
    LOGGER.info("      The name of topic to say that the device is alive:          " + String(settings.topicToListenCommands) + "/" + String(settings.mqttDeviceName));
    LOGGER.info("      The name of topic to listen when server has born again:     " + String(settings.topicToListenServerWasBorn));
    LOGGER.info("      The name of topic to report the device enabled:             " + String(settings.topicIsTheDeviceEnabled) + "/" + String(settings.mqttDeviceName));
    LOGGER.info("      Pressure update difference (Atm):                           " + String(settings.pressureUpdateDiffAtm));
    LOGGER.info("      Pressure publish min interval (sec):                        " + String(settings.pressurePubMinIntSec));
    LOGGER.info("      Pump state publish min interval (sec):                      " + String(settings.pumpStatePubMinIntSec));
    LOGGER.info("   pressure:");
    LOGGER.info("      minMpa: " + String(settings.pressure.minMpa));
    LOGGER.info("      maxMpa: " + String(settings.pressure.maxMpa));
    LOGGER.info("      leakDetectSec: " + String(settings.pressure.leakDetectSec));
    LOGGER.info("      leakDetectEnabled: " + String(settings.pressure.leakDetectEnabled ? "true" : "false"));
    LOGGER.info("      pumpWeakSec: " + String(settings.pressure.pumpWeakSec));
    LOGGER.info("      sensorMaxMpa: " + String(settings.pressure.sensorMaxMpa));
    LOGGER.info("      sensorMinVolts: " + String(settings.pressure.sensorMinVolts));
    LOGGER.info("      sensorMaxVolts: " + String(settings.pressure.sensorMaxVolts));
    LOGGER.info("      samplesCount: " + String(settings.pressure.samplesCount));
    LOGGER.info("      measureIntervalMs: " + String(settings.pressure.measureIntervalMs));
    LOGGER.info("      measurementsCount: " + String(settings.pressure.measurementsCount));
}
//--------------------------------------------------------------------
