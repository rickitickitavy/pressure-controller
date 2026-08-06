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
public:
    using PumpControlFn = void (*)(bool enabled);

private:
    AsyncWebServer *webServer;

    static PumpControlFn pumpControlHandler;
    static bool devicePumpOn;
    static bool devicePumpControlEnabled;
    static bool deviceMqttConnected;
    static bool httpUploadInProgress;
    static int uploadPartitionType;

    static bool isOtaAllowed();
    static void statusApiProcessor(AsyncWebServerRequest *request);
    static void updateCodePostProcessor(AsyncWebServerRequest *request);
    static void updateDataPostProcessor(AsyncWebServerRequest *request);
    static void updateUploadHandler(AsyncWebServerRequest *request, const String &filename, size_t index,
                                    uint8_t *data, size_t len, bool final);

public:
    static SettingsManager *settingsManager;

    WebServerController(SettingsManager *settingsManager);

    static String systemSettingsProcessor(const String &paramName);

    static void settingsApiProcessor(AsyncWebServerRequest *request);

    static void sensorVoltageProcessor(AsyncWebServerRequest *request);

    static void loadFileByUrl(AsyncWebServerRequest *request);

    static void setPumpControlHandler(PumpControlFn fn);

    static void setDeviceStatus(bool pumpOn, bool pumpControlEnabled, bool mqttConnected);

    static bool isHttpUploadInProgress();
};

extern String TEXT_PLAN;
#endif //EFLAMEESP8266_WEBSERVERCONTROLLER_H
