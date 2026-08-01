//
// Created by dsporykhin on 24.04.20.
//

#ifndef EFLAMEESP8266_SETTINGSNAVIGATOR_H
#define EFLAMEESP8266_SETTINGSNAVIGATOR_H


#include "SettingsManager.h"
#include "ParamDescriptor.h"

class ParamDescriptor;

class SettingsManager;

/**
 * Класс для чтения и сохранения настроек по их именам
 */
class SettingsNavigator {
private:
    SettingsManager *settingsManager;
    GlobalSettings *settings;
    ParamDescriptor *paramDescriptors[50];
    int activeParamDescriptors = 0;

    String saveSettingByName(String paramName, String value);


public:
    SettingsNavigator(SettingsManager *settingsManager);


    GlobalSettings tempSettings;

    /**
     * Сохранить новые сетевые настройки и перезагрузиться с ними
     * @param networkSettings
     */
    void saveNetworkSettingsAndRestart(NetworkSettings *networkSettings);

    /**
     * Возвращает значение параметра по имени
     * @param paramName
     * @return
     */
    String getSettingByName(String paramName);

    /**
     * Сохранить значения параметров.
     * @param paramNames
     * @param values
     * @return
     */
    String saveSettingsByNames(String *params, int paramsCount);

};


#endif //EFLAMEESP8266_SETTINGSNAVIGATOR_H
