//
// Created by dsporykhin on 19.04.20.
//

#include "WebServerController.h"
#include "SettingsManager.h"

#ifndef EFLAMEESP8266_WIFICONTROLLER_H
#define EFLAMEESP8266_WIFICONTROLLER_H


class WiFiController {
private:
    static constexpr unsigned long kApTimeoutMs = 5UL * 60UL * 1000UL;
    static constexpr unsigned long kStaReconnectMs = 10UL * 1000UL;

    SettingsManager* settingsManager;
    WebServerController* serverController;
    bool forceAp;
    bool apActive = false;
    unsigned long apStartedMs = 0;
    unsigned long lastStaReconnectMs = 0;
    bool staWasConnected = false;

    void init();

    String getTextErrorStatus();

    void startAp(const String& deviceName);

    void startSta(const String& deviceName);

    void stopAp();

    void reconnectSta();

    void onStaConnected();

public:
    WiFiController(SettingsManager* settingsManager, bool forceAp);

    bool isApMode() const;

    /**
     * AP timeout + STA reconnect / post-reconnect radio recovery.
     */
    void update();
};


#endif //EFLAMEESP8266_WIFICONTROLLER_H
