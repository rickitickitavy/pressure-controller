//
// Created by dsporykhin on 19.04.20.
//

#include "WebServerController.h"
#include "Defines.h"
#include "GlobalSettings.h"
#include "Logger.h"
#include "pressure.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <Update.h>
#include <WiFi.h>

String TEXT_PLAN = "text/plan";
String TEXT_JSON = "text/json";
String OK_RESPONSE = "OK";
String PROFILES_PARAMETER_ATTR_NAME = "parameter";

SettingsManager *WebServerController::settingsManager;
WebServerController::PumpControlFn WebServerController::pumpControlHandler = nullptr;
bool WebServerController::devicePumpOn = false;
bool WebServerController::devicePumpControlEnabled = true;
bool WebServerController::deviceMqttConnected = false;
bool WebServerController::httpUploadInProgress = false;
int WebServerController::uploadPartitionType = U_FLASH;

namespace {
bool parseTruthy(const String &value) {
    String v = value;
    v.toLowerCase();
    return v == "1" || v == "true" || v == "yes" || v == "on";
}
} // namespace

WebServerController::WebServerController(SettingsManager *settingsManager) {
    WebServerController::settingsManager = settingsManager;

    LOGGER.info(" Starting web server...");

    webServer = new AsyncWebServer(80);

    webServer->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/index.html", String(), false, systemSettingsProcessor);
    });

    webServer->on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/index.html", String(), false, systemSettingsProcessor);
    });

    webServer->on("/log", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", &LOGGER.logData[0]);
    });

    webServer->on("/settingsApi", HTTP_GET, settingsApiProcessor);
    webServer->on("/settingsApi", HTTP_POST, settingsApiProcessor);

    webServer->on("/statusApi", HTTP_GET, statusApiProcessor);
    webServer->on("/statusApi", HTTP_POST, statusApiProcessor);

    webServer->on("/sensorVoltage", HTTP_GET, sensorVoltageProcessor);

    webServer->on(
            "/update/code", HTTP_POST, updateCodePostProcessor,
            [](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len,
               bool final) { updateUploadHandler(request, filename, index, data, len, final); });

    webServer->on(
            "/update/data", HTTP_POST, updateDataPostProcessor,
            [](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len,
               bool final) { updateUploadHandler(request, filename, index, data, len, final); });

    webServer->onNotFound(loadFileByUrl);

    webServer->begin();
}

void WebServerController::setPumpControlHandler(PumpControlFn fn) {
    pumpControlHandler = fn;
}

void WebServerController::setDeviceStatus(bool pumpOn, bool pumpControlEnabled, bool mqttConnected) {
    devicePumpOn = pumpOn;
    devicePumpControlEnabled = pumpControlEnabled;
    deviceMqttConnected = mqttConnected;
}

bool WebServerController::isHttpUploadInProgress() {
    return httpUploadInProgress;
}

bool WebServerController::isOtaAllowed() {
    // HTTP upload is gated only by reachability of the web UI, not enableOtaOnNetwork
    // (that flag still controls ArduinoOTA only).
    const wifi_mode_t mode = WiFi.getMode();
    if (mode == WIFI_AP || mode == WIFI_AP_STA) {
        return true;
    }
    return WiFi.isConnected();
}

void WebServerController::statusApiProcessor(AsyncWebServerRequest *request) {
    if (request->method() == HTTP_GET) {
        const float pressureAtm = PRESSURE.readMpa() / PRESSURE_ATM_MPA;
        const wifi_mode_t mode = WiFi.getMode();
        const bool apMode = mode == WIFI_AP || mode == WIFI_AP_STA;
        const bool wifiConnected = WiFi.isConnected();
        const IPAddress ip = apMode ? WiFi.softAPIP() : WiFi.localIP();

        String json = "{";
        json += "\"pressureAtm\":" + String(pressureAtm, 2) + ",";
        json += "\"pumpOn\":" + String(devicePumpOn ? "true" : "false") + ",";
        json += "\"pumpControlEnabled\":" + String(devicePumpControlEnabled ? "true" : "false") + ",";
        json += "\"firmwareVersion\":\"" + String(FIRMWARE_VERSION) + "\",";
        json += "\"ip\":\"" + ip.toString() + "\",";
        json += "\"rssi\":" + String(wifiConnected ? WiFi.RSSI() : 0) + ",";
        json += "\"mqttConnected\":" + String(deviceMqttConnected ? "true" : "false");
        json += "}";

        request->send(200, TEXT_JSON, json);
        return;
    }

    if (request->method() == HTTP_POST) {
        // Prefer POST body params; fall back to query string (more reliable for small toggles).
        String operation = request->arg(PROFILES_OPERATION_ATTR_NAME);
        if (operation.isEmpty() && request->hasParam(PROFILES_OPERATION_ATTR_NAME, true)) {
            operation = request->getParam(PROFILES_OPERATION_ATTR_NAME, true)->value();
        }
        if (operation != PARAMETER_OPERATION_WRITE) {
            request->send(400, TEXT_PLAN, "Unsupported operation");
            return;
        }

        String enabledArg = request->arg("enabled");
        if (enabledArg.isEmpty() && request->hasParam("enabled", true)) {
            enabledArg = request->getParam("enabled", true)->value();
        }
        if (enabledArg.isEmpty()) {
            request->send(400, TEXT_PLAN, "\"enabled\" is required");
            return;
        }
        const bool enabled = parseTruthy(enabledArg);
        if (pumpControlHandler == nullptr) {
            request->send(503, TEXT_PLAN, "Pump control unavailable");
            return;
        }
        LOGGER.info(String("Web statusApi: pumpControlEnabled=") + (enabled ? "true" : "false"));
        pumpControlHandler(enabled);
        // Reflect immediately so the next poll does not race the main-loop snapshot.
        devicePumpControlEnabled = enabled;
        if (!enabled) {
            devicePumpOn = false;
        }
        request->send(200, TEXT_PLAN, OK_RESPONSE);
    }
}

void WebServerController::updateUploadHandler(AsyncWebServerRequest *request, const String &filename, size_t index,
                                              uint8_t *data, size_t len, bool final) {
    (void) filename;

    if (index == 0) {
        uploadPartitionType = request->url().endsWith("/data") ? U_SPIFFS : U_FLASH;
        if (httpUploadInProgress) {
            LOGGER.error("HTTP OTA rejected: upload already in progress");
            return;
        }
        if (!isOtaAllowed()) {
            LOGGER.error("HTTP OTA rejected: not allowed in current WiFi/OTA mode");
            return;
        }
        httpUploadInProgress = true;
        LOGGER.info("HTTP OTA start, partition=" + String(uploadPartitionType));
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, uploadPartitionType)) {
            Update.printError(Serial);
            httpUploadInProgress = false;
            return;
        }
    }

    if (len > 0) {
        if (Update.write(data, len) != len) {
            Update.printError(Serial);
        }
    }

    if (final) {
        if (!Update.end(true)) {
            Update.printError(Serial);
            httpUploadInProgress = false;
        }
    }
}

void WebServerController::updateCodePostProcessor(AsyncWebServerRequest *request) {
    (void) request;
    if (Update.hasError() || !httpUploadInProgress) {
        const String message = Update.hasError() ? Update.errorString() : "Upload failed";
        request->send(500, TEXT_PLAN, message);
        Update.abort();
        httpUploadInProgress = false;
        return;
    }
    request->send(200, TEXT_PLAN, OK_RESPONSE);
    delay(500);
    ESP.restart();
}

void WebServerController::updateDataPostProcessor(AsyncWebServerRequest *request) {
    (void) request;
    if (Update.hasError() || !httpUploadInProgress) {
        const String message = Update.hasError() ? Update.errorString() : "Upload failed";
        request->send(500, TEXT_PLAN, message);
        Update.abort();
        httpUploadInProgress = false;
        return;
    }
    request->send(200, TEXT_PLAN, OK_RESPONSE);
    delay(500);
    ESP.restart();
}

/**
 * Change system settings handler
 * @param request
 */
void WebServerController::settingsApiProcessor(AsyncWebServerRequest *request) {

    LOGGER.info("apiSettings");

    String parameterAttrName = "parameter";
    String valueAttrName = "value";

    String operation = request->arg(PROFILES_OPERATION_ATTR_NAME);
    String parameter = request->arg(parameterAttrName);

    if (!parameter || parameter.isEmpty()) {
        String message = "\"" + parameterAttrName + "\" can't be null";
        request->send(400, TEXT_PLAN, message);
    } else if (parameter == "wifi") {
        if (operation = PARAMETER_OPERATION_WRITE) {
            LOGGER.info("all setting.");

            String wifiSSID = request->arg("network>ssid");
            String wifiPassword = request->arg("network>password");
            String wifiHostName = request->arg("network>hostName");
            String mqttServer = request->arg("mqtt>server");
            String mqttPort = request->arg("mqtt>port");
            String mqttDeviceName = request->arg("mqtt>deviceName");
            String mqttReconnectIntervalMs = request->arg("mqtt>reconnectIntervalMs");

            LOGGER.info("   start check");

            if (!wifiSSID || wifiSSID.isEmpty()) {
                request->send(400, TEXT_PLAN, "wiFiSsId can't be null");
            } else if (!wifiPassword || wifiPassword.isEmpty()) {
                request->send(400, TEXT_PLAN, "wiFiPassword can't be null");
            } else {
                LOGGER.info("   saving...");

                NetworkSettings networkSettings;
                wifiSSID.toCharArray(&networkSettings.ssid[0], sizeof(networkSettings.ssid));
                wifiPassword.toCharArray(&networkSettings.password[0],
                                         sizeof(networkSettings.password));
                wifiHostName.toCharArray(&networkSettings.hostName[0],
                                         sizeof(networkSettings.hostName));

                String enableOta = request->arg("network>enableOtaOnNetwork");
                enableOta.toLowerCase();
                networkSettings.enableOtaOnNetwork =
                        enableOta == "1" || enableOta == "true" || enableOta == "yes" ||
                        enableOta == "on";

                String wifiEnabled = request->arg("network>wifiEnabled");
                wifiEnabled.toLowerCase();
                networkSettings.wifiEnabled =
                        wifiEnabled == "1" || wifiEnabled == "true" || wifiEnabled == "yes" ||
                        wifiEnabled == "on";

                request->send(200, TEXT_PLAN, OK_RESPONSE);

                settingsManager->getNavigator()->saveNetworkSettingsAndRestart(&networkSettings);
                LOGGER.info("   saved");
            }
        } else {
            String message = "Read operation for \"wifi\" block unsupported. Use read for every parameter";
            LOGGER.error(message);
            request->send(400, TEXT_PLAN, message);
        }
    } else {
        if (operation == PARAMETER_OPERATION_READ) {
            String message = settingsManager->getNavigator()->getSettingByName(parameter);
            request->send(200, TEXT_PLAN, message);
        } else {
            // Запись параметров

            //  Подсчет кол-ва параметров
            int paramsCount = 1;
            int indexOfParamSeparator = -1;
            while ((indexOfParamSeparator = parameter.indexOf('|', indexOfParamSeparator + 1)) >= 0)
                paramsCount++;

            String params[paramsCount];

            // Соберем массивы
            indexOfParamSeparator = parameter.indexOf('|');
            int startCopyIndex = 0;
            int paramIndex = 0;
            if (parameter.indexOf('|') >= 0)
                do {
                    params[paramIndex++] = indexOfParamSeparator == -1
                                           ? parameter.substring(startCopyIndex)
                                           : parameter.substring(startCopyIndex, indexOfParamSeparator);
                    startCopyIndex = indexOfParamSeparator + 1;
                } while ((indexOfParamSeparator = parameter.indexOf('|', indexOfParamSeparator + 1)) >= 0);
            // последний элемент
            if (startCopyIndex < parameter.length()) {
                params[paramIndex] = indexOfParamSeparator == -1
                                     ? parameter.substring(startCopyIndex)
                                     : parameter.substring(startCopyIndex, indexOfParamSeparator);
            }

            for (int i = 0; i < paramsCount; LOGGER.debug(params[i++]));

            String message = settingsManager->getNavigator()->saveSettingsByNames(&params[0], paramsCount);

            if (message.isEmpty()) {
                request->send(200, TEXT_PLAN, OK_RESPONSE);
            } else {
                request->send(400, TEXT_PLAN, message);
                LOGGER.error(message);
            }

        }
    }

}
//----------------------------------------------------------------------

void WebServerController::sensorVoltageProcessor(AsyncWebServerRequest *request) {
    request->send(200, TEXT_PLAN, String(PRESSURE.readVolts(), 3));
}
//----------------------------------------------------------------------


void WebServerController::loadFileByUrl(AsyncWebServerRequest *request) {
    String url = request->url();
    String mime;

    File testFile = LittleFS.open(request->url(), "r");
    if (!testFile) {

        LOGGER.error("url not found: \"" + request->url() + "\"");

        request->send(404, TEXT_PLAN, "not found");
    } else {
        testFile.close();

        if (url.endsWith(".html")) {
            mime = "text/html";
        } else if (url.endsWith(".css")) {
            mime = "text/css";
        } else if (url.endsWith(".js")) {
            mime = "text/js";
        } else if (url.endsWith(".jpg")) {
            mime = "image/jpeg";
        } else {
            mime = TEXT_PLAN;
        }

        request->send(LittleFS, url, mime);
    }
}
//----------------------------------------------------------------------


String WebServerController::systemSettingsProcessor(const String &paramName) {
    return settingsManager->getNavigator()->getSettingByName(paramName);
}
//----------------------------------------------------------------------
