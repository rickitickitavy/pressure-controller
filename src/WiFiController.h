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
    bool forceAp;

    void init();

    String getTextErrorStatus();

    void startAp(const String& deviceName);
public:
    WiFiController(SettingsManager* settingsManager, bool forceAp);

    bool isApMode() const;
};

// extern WiFiController* wiFiController;
#endif //EFLAMEESP8266_WIFICONTROLLER_H
