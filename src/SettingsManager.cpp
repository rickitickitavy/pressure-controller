#include "SettingsNavigator.h"//
#include "Logger.h"
// Created by dsporykhin on 23.04.20.
//

#include <EEPROM.h>
#include <math.h>

SettingsManager::SettingsManager(){

    EEPROM.begin(4096);
    LOGGER.info("Load settings...");
    readSettings();
    navigator = new SettingsNavigator(this);

    if ((settings.initMarker[0] != GLOBAL_SETTINGS_MARKER_0)
        || (settings.initMarker[1] != GLOBAL_SETTINGS_MARKER_1)
        || (settings.initMarker[2] != GLOBAL_SETTINGS_MARKER_2)
        || (settings.initMarker[3] != GLOBAL_SETTINGS_MARKER_3)) {
        // настройки не инициализированы
        settings.initMarker[0] = GLOBAL_SETTINGS_MARKER_0;
        settings.initMarker[1] = GLOBAL_SETTINGS_MARKER_1;
        settings.initMarker[2] = GLOBAL_SETTINGS_MARKER_2;
        settings.initMarker[3] = GLOBAL_SETTINGS_MARKER_3;

        LOGGER.error("Settings has never been initialized. Initializing by default config...");

        settings.version = GLOBAL_CURRENT_SETTINGS_VERSION;

        // Заполнение дефолтными значениями
        resetWiFi();

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

        String("device").toCharArray(settings.mqttDeviceName, sizeof(settings.mqttDeviceName));

        settings.bright = 100.0f;
        applyPressureDefaults();

        saveSetting(true);
        LOGGER.warning("Settings has never been initialized");
    } else {

        LOGGER.info("Settings loaded. Version " + String(settings.version));
        if (settings.version != GLOBAL_CURRENT_SETTINGS_VERSION) {

            LOGGER.warning("Upgrade settings to version " + String(GLOBAL_CURRENT_SETTINGS_VERSION));

            settings.bright = 100.0f;
            applyPressureDefaults();
            String(DEFAULT_TOPIC_PRESSURE_VALUE).toCharArray(
                    settings.topicPressureValue, sizeof(settings.topicPressureValue));

            settings.version = GLOBAL_CURRENT_SETTINGS_VERSION;
            LOGGER.warning(" Upgrade settings finished");
            saveSetting(false);
        } else {
            clampAdvanced(settings.pressure);
            clampPair(settings.pressure.minMpa, settings.pressure.maxMpa, settings.pressure.sensorMaxMpa);
        }
    }


    logSettings();
};
//--------------------------------------------------------------------

void SettingsManager::applyPressureDefaults() {
    settings.pressure = PressureSettings{};
    clampAdvanced(settings.pressure);
    clampPair(settings.pressure.minMpa, settings.pressure.maxMpa, settings.pressure.sensorMaxMpa);
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

    delay(20);
    if (restart) {
        LOGGER.warning("RESTARTING...");
        ESP.restart();
    }
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
    LOGGER.info("   mqtt:");
    LOGGER.info("      server: " + String(settings.mqttServer));
    LOGGER.info("      port: " + String(settings.mqttPort));
    LOGGER.info("      reconIntervalMs: " + String(settings.mqttReconnectIntervalMs));
    LOGGER.info("      device name: " + String(settings.mqttDeviceName));
    LOGGER.info("      The name of the topic to report that the device is alive:   " + String(settings.topicTheDeviceIsAlive));
    LOGGER.info("      The name of topic to report the pump state:                 " + String(settings.topicThePumpState) + "/" + String(settings.mqttDeviceName));
    LOGGER.info("      The name of topic to report the pressure value:             " + String(settings.topicPressureValue) + "/" + String(settings.mqttDeviceName));
    LOGGER.info("      The name of topic to say that the device is alive:          " + String(settings.topicToListenCommands) + "/" + String(settings.mqttDeviceName));
    LOGGER.info("      The name of topic to listen when server has born again:     " + String(settings.topicToListenServerWasBorn));
    LOGGER.info("   device:");
    LOGGER.info("      bright: " + String(settings.bright));
    LOGGER.info("   pressure:");
    LOGGER.info("      minMpa: " + String(settings.pressure.minMpa));
    LOGGER.info("      maxMpa: " + String(settings.pressure.maxMpa));
    LOGGER.info("      leakDetectSec: " + String(settings.pressure.leakDetectSec));
    LOGGER.info("      pumpWeakSec: " + String(settings.pressure.pumpWeakSec));
    LOGGER.info("      sensorMaxMpa: " + String(settings.pressure.sensorMaxMpa));
}
//--------------------------------------------------------------------
