//
// Created by dsporykhin on 19.04.20.
//

#include <WiFi.h>
#include "WiFiController.h"
#include "Defines.h"
#include "Logger.h"


WiFiController::WiFiController(SettingsManager *settingsManager, bool forceAp) {
    this->settingsManager = settingsManager;
    this->forceAp = forceAp;
    init();
}

bool WiFiController::isApMode() const {
    const wifi_mode_t mode = WiFi.getMode();
    return mode == WIFI_AP || mode == WIFI_AP_STA;
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

void WiFiController::startAp(const String& deviceName) {
    LOGGER.info("Starting WiFi AP mode...");

    // Clean shut-down: ESP32-C3 is sensitive to leftover STA state.
    WiFi.persistent(false);
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    delay(500);

    WiFi.mode(WIFI_AP);
    delay(500);

    String ssid = deviceName.length() > 0 ? (deviceName + "-WiFi") : String("pump-WiFi");
    if (ssid.length() > 32) {
        ssid = ssid.substring(0, 32);
    }
    LOGGER.info("WiFi AP name is " + ssid);

    // softAPConfig must run before softAP on ESP32-C3.
    const IPAddress apIp(192, 168, 0, 1);
    const IPAddress gateway(192, 168, 0, 1);
    const IPAddress subnet(255, 255, 255, 0);
    if (!WiFi.softAPConfig(apIp, gateway, subnet)) {
        LOGGER.error("softAPConfig failed");
    }

    // Channel 6 + reduced TX power: known ESP32-C3 SoftAP visibility fixes.
    const bool ok = WiFi.softAP(ssid.c_str(), "00000000", 6, 0, 4);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    delay(200);

    if (ok) {
        LOGGER.info("WiFi AP started");
    } else {
        LOGGER.error("WiFi softAP FAILED");
    }
    LOGGER.info("SSID: " + String(WiFi.softAPSSID()));
    LOGGER.info("AP IP: " + WiFi.softAPIP().toString());
}

void WiFiController::init() {
    GlobalSettings *settings = settingsManager->getSettings();
    String deviceName = String(settings->mqttDeviceName);

    LOGGER.info("Configuring WiFi...");

    if (forceAp) {
        startAp(deviceName);
    } else {
        // Clean STA start — ESP32-C3 is sensitive to leftover WiFi state / TX power.
        WiFi.persistent(false);
        WiFi.disconnect(true, true);
        WiFi.mode(WIFI_OFF);
        delay(500);

        WiFi.mode(WIFI_STA);
        delay(200);
        WiFi.setSleep(false);
        WiFi.setHostname(deviceName.c_str());

        LOGGER.info("Connecting to SSID '" + String(settings->network.ssid) + "'...");
        WiFi.begin(settings->network.ssid, settings->network.password);
        // Many ESP32-C3 boards (SuperMini / weak LDO / antenna) need reduced TX power.
        WiFi.setTxPower(WIFI_POWER_8_5dBm);

        const unsigned long timeoutMs = 20000;
        const unsigned long startedAt = millis();
        wl_status_t lastStatus = WL_IDLE_STATUS;
        while (!WiFi.isConnected() && (millis() - startedAt < timeoutMs)) {
            const wl_status_t status = WiFi.status();
            if (status != lastStatus) {
                lastStatus = status;
                LOGGER.info("WiFi status:" + getTextErrorStatus());
            }
            delay(100);
        }
        if (!WiFi.isConnected()) {
            LOGGER.error("Not connected. " + getTextErrorStatus() + ". Staying in STA mode (no AP fallback).");
        } else {
            // Re-apply after association — some stacks reset TX power on connect.
            WiFi.setTxPower(WIFI_POWER_8_5dBm);
            Serial.println("Connected to router.");
            Serial.println("local IP " + WiFi.localIP().toString());
            Serial.println("hostname " + deviceName);
            LOGGER.info("RSSI: " + String(WiFi.RSSI()) + " dBm, TX power: " + String(WiFi.getTxPower()));
        }
    }

    LOGGER.info("IP " + (isApMode() ? WiFi.softAPIP() : WiFi.localIP()).toString());
    LOGGER.info("MAC " + WiFi.macAddress());
    LOGGER.info("WiFi current state: " + String(WiFi.getMode()));

    serverController = new WebServerController(settingsManager);
}
