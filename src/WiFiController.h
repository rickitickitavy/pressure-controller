//
// Created by dsporykhin on 19.04.20.
//

#include "WebServerController.h"
#include "SettingsManager.h"

#ifndef EFLAMEESP8266_WIFICONTROLLER_H
#define EFLAMEESP8266_WIFICONTROLLER_H


class WiFiController {
private:
    SettingsManager* settingsManager;
    WebServerController* serverController;

    void init();

    String getTextErrorStatus();
public:
    WiFiController(SettingsManager* settingsManager);
};

// extern WiFiController* wiFiController;
#endif //EFLAMEESP8266_WIFICONTROLLER_H
