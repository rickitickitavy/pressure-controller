#include "SettingsNavigator.h"//
#include "Logger.h"
// Created by dsporykhin on 23.04.20.
//

#include <EEPROM.h>

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

        String(DEFAULT_TOPIC_THE_DEVICE_STATE).toCharArray(
                settings.topicTheDeviceState, sizeof(settings.topicTheDeviceState));

        String(DEFAULT_TOPIC_TO_LISTEN_COMMANDS).toCharArray(
                settings.topicToListenCommands, sizeof(settings.topicToListenCommands));

        String(DEFAULT_TOPIC_TO_LISTEN_SERVER_WAS_BORN).toCharArray(
                settings.topicToListenServerWasBorn, sizeof(settings.topicToListenServerWasBorn));

        String("device").toCharArray(settings.mqttDeviceName, sizeof(settings.mqttDeviceName));

        settings.bright = 100.0f;

        saveSetting(true);
        LOGGER.warning("Settings has never been initialized");
    } else {

        LOGGER.info("Settings loaded. Version " + String(settings.version));
        if (settings.version != GLOBAL_CURRENT_SETTINGS_VERSION) {

            LOGGER.warning("Upgrade settings to version " + String(GLOBAL_CURRENT_SETTINGS_VERSION));

            settings.bright = 100.0f;

            settings.version = GLOBAL_CURRENT_SETTINGS_VERSION;
            LOGGER.warning(" Upgrade settings finished");
            saveSetting(false);
        }
    }


    logSettings();
};
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
    LOGGER.info("      The name of topic to report the device state:               " + String(settings.topicTheDeviceState) + "/" + String(settings.mqttDeviceName));
    LOGGER.info("      The name of topic to say that the device is alive:          " + String(settings.topicToListenCommands) + "/" + String(settings.mqttDeviceName));
    LOGGER.info("      The name of topic to listen when server has born again:     " + String(settings.topicToListenServerWasBorn));
    LOGGER.info("   device:");
    LOGGER.info("      bright: " + String(settings.bright));
}
//--------------------------------------------------------------------
