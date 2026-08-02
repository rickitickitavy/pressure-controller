//
// Created by dsporykhin on 19.04.20.
//

#ifndef EFLAMEESP8266_WEBSERVERCONTROLLER_H
#define EFLAMEESP8266_WEBSERVERCONTROLLER_H

#define PARAMETER_OPERATION_WRITE "write"
#define PARAMETER_OPERATION_READ "read"

#define PROFILES_OPERATION_ATTR_NAME "operation"
#define PROFILES_NAME_ATTR_NAME "profileName"
#define PROFILES_OPERATION_GET_LIST "getList"
#define PROFILES_OPERATION_ACTIVATE "activate"
#define PROFILES_OPERATION_SAVE "save"
#define PROFILES_OPERATION_REMOVE "remove"

#include <ESPAsyncWebServer.h>
#include "SettingsManager.h"

class WebServerController {
private:
    AsyncWebServer* webServer;
public:
    static SettingsManager* settingsManager;
    WebServerController(SettingsManager* settingsManager);

    static String systemSettingsProcessor(const String& paramName);

    static void settingsApiProcessor(AsyncWebServerRequest *request);

    static void sensorVoltageProcessor(AsyncWebServerRequest *request);

    static void loadFileByUrl(AsyncWebServerRequest *request);

};

extern String TEXT_PLAN;
#endif //EFLAMEESP8266_WEBSERVERCONTROLLER_H
