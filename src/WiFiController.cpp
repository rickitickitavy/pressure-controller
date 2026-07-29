//
// Created by dsporykhin on 19.04.20.
//

#include <WiFi.h>
#include "WiFiController.h"
#include "Defines.h"
#include "Logger.h"


WiFiController::WiFiController(SettingsManager *settingsManager) {
    this->settingsManager = settingsManager;
    init();
}

String WiFiController::getTextErrorStatus() {
    switch (WiFi.status()) {
        case WL_CONNECTED:
            return "   WL_CONNECTED";
        case WL_NO_SSID_AVAIL:
            return "   WL_NO_SSID_AVAIL";
        case WL_CONNECT_FAILED:
            return "   WL_CONNECT_FAILED";
        case WL_IDLE_STATUS:
            return "   WL_IDLE_STATUS";
        case WL_CONNECTION_LOST:
            return "   WL_CONNECTION_LOST";
        case WL_DISCONNECTED:
            return "   WL_DISCONNECTED";
        default:
            return "   WL_UNKNOWN";
    }
}

void WiFiController::init() {
    GlobalSettings *settings = settingsManager->getSettings();
    String deviceName = String(settings->mqttDeviceName);

    LOGGER.info("Configuring WiFi if needed...");

    if ((String(WiFi.getHostname()) != deviceName)
            || (WiFi.getMode() != WIFI_STA)) {

        LOGGER.info("Configuring WiFi...");

        WiFi.mode(WIFI_OFF);
        delay(300);

        // Hostname after STA, before begin — otherwise DHCP omits the name
        WiFi.mode(WIFI_STA);
        WiFi.setHostname(deviceName.c_str());
        WiFi.begin(settings->network.ssid, settings->network.password);

        long startedAt = millis();
        while (!WiFi.isConnected() && (millis() - startedAt < 10000)) {
            delay(20);
        }
        if (!WiFi.isConnected()) {
            // не подключились. В режим AP

            LOGGER.error("Not connected. " + getTextErrorStatus() + ". Switching to AP mode...");

            WiFi.mode(WIFI_AP);
            delay(300);
            WiFi.setHostname(deviceName.c_str());
            WiFi.softAP(deviceName + "-WiFi", "00000000");
            delay(20);
            WiFi.softAPConfig(IPAddress(192, 168, 0, 1), IPAddress(192, 168, 0, 1),
                              IPAddress(255, 255, 255, 0));
            delay(20);
            LOGGER.info("    switching to AP mode...");


        } else {
            Serial.println("Connected to router.");
            Serial.println("local IP " + WiFi.localIP().toString());
            Serial.println("hostname " + deviceName);
        }

        LOGGER.info("IP " + (WiFi.getMode() == WIFI_AP ? WiFi.softAPIP() : WiFi.localIP()).toString());
        LOGGER.info("MAC " + WiFi.macAddress());

        LOGGER.info("WiFi current state: " + String(WiFi.getMode()));

    }

    serverController = new WebServerController(settingsManager);

}
