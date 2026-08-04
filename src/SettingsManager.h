//
// Created by dsporykhin on 23.04.20.
//

#ifndef EFLAMEESP8266_SETTINGSMANAGER_H
#define EFLAMEESP8266_SETTINGSMANAGER_H

#include <WString.h>
#include "GlobalSettings.h"
#include "SettingsNavigator.h"

class SettingsNavigator;
/**
 * Клас для управления настройками
 */
class SettingsManager {
private:
    GlobalSettings settings;

    SettingsNavigator* navigator;

    bool pendingRestart = false;
    unsigned long restartRequestedMs = 0;

    /**
     * Загрузка настроек из EEPROM
     * @param settings
     */
    void readSettings();

    void applyDefaults();

public:
    SettingsManager();

    int pinValues[4] = {0, 0, 0, 0};

    /**
     * Загрузка настроек в произвольный буфер настроек
     * @param settings
     */
    void readSettings(GlobalSettings* settings);


        /**
         * Возвращает навигатор по настройкам
         * @return
         */
    SettingsNavigator* getNavigator();

    /**
     * Сохранение настроек в EEPROM и, при необходимости, запрос перезапуска (из loop).
     * @param restart
     */
    void saveSetting(bool restart);

    /**
     * Сохранить настройки из специфического буфера
     * @param settingsToSave
     * @param restart when true, schedules ESP.restart() for the main loop (not here)
     */
    void saveSetting(GlobalSettings* settingsToSave, bool restart);

    /**
     * If a restart was requested by saveSetting(true), wait until delayMs elapsed
     * since the request, then ESP.restart(). Call from setup/loop only.
     * @return true if a restart was performed (does not return on success)
     */
    bool handlePendingRestart(unsigned long delayMs);

    /**
     * Save pump/pressure settings into EEPROM (no restart)
     */
    void savePressure(const PressureSettings& pressure);

    static void clampAdvanced(PressureSettings& s);

    static void clampPair(float& minMpa, float& maxMpa, float sensorMaxMpa);

    static void clampPressureUpdateDiff(float& diffAtm);

    static void clampMqttPublishMinInterval(int& intervalSec);

    static void clampMqttClientTimeout(int& timeoutMs);

    /**
     * Сброс настроек WiFi в дефолтовые
     */
    void resetWiFi();

    /**
     * Возвражает ссылку на структуру настроек
     * @return
     */
    GlobalSettings* getSettings();


    void logSettings();
};


#endif //EFLAMEESP8266_SETTINGSMANAGER_H
