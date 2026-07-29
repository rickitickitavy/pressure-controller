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
    return apActive;
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
        apActive = true;
        apStartedMs = millis();
        LOGGER.info("WiFi AP started (auto-off in 5 min)");
    } else {
        apActive = false;
        LOGGER.error("WiFi softAP FAILED");
    }
    LOGGER.info("SSID: " + String(WiFi.softAPSSID()));
    LOGGER.info("AP IP: " + WiFi.softAPIP().toString());
}

void WiFiController::stopAp() {
    if (!apActive) {
        return;
    }
    LOGGER.warning("Stopping WiFi AP mode...");
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(300);
    apActive = false;
}

void WiFiController::onStaConnected() {
    // Auto-reconnect / association often re-enables modem sleep and resets TX power,
    // leaving the device "connected" but unreachable (ping fails).
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_15dBm);
    LOGGER.info("STA ready. IP " + WiFi.localIP().toString() +
                ", GW " + WiFi.gatewayIP().toString() +
                ", RSSI " + String(WiFi.RSSI()) + " dBm");
}

void WiFiController::reconnectSta() {
    GlobalSettings *settings = settingsManager->getSettings();

    LOGGER.warning("STA reconnecting to '" + String(settings->network.ssid) + "'...");

    WiFi.setSleep(false);
    // Drop association but keep mode; force a fresh DHCP lease so the router
    // does not keep a stale ARP entry for an IP we no longer own cleanly.
    WiFi.disconnect(false, false);
    delay(100);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.begin(settings->network.ssid, settings->network.password);
    WiFi.setTxPower(WIFI_POWER_15dBm);
}

void WiFiController::startSta(const String& deviceName) {
    GlobalSettings *settings = settingsManager->getSettings();

    WiFi.persistent(false);
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    delay(500);

    WiFi.mode(WIFI_STA);
    delay(200);
    WiFi.setSleep(false);
    // Manual reconnect in update() — autoReconnect can leave STA associated
    // without a working IP stack (device unreachable by ping).
    WiFi.setAutoReconnect(false);
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
    if (!WiFi.isConnected() || WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
        LOGGER.error("Not connected. " + getTextErrorStatus() + ". Staying in STA mode (no AP fallback).");
        staWasConnected = false;
    } else {
        onStaConnected();
        staWasConnected = true;
        Serial.println("Connected to router.");
        Serial.println("local IP " + WiFi.localIP().toString());
        Serial.println("hostname " + deviceName);
    }
    lastStaReconnectMs = millis();
}

void WiFiController::update() {
    if (apActive) {
        if ((millis() - apStartedMs) < kApTimeoutMs) {
            return;
        }

        LOGGER.warning("AP 5-minute timeout expired");
        const String deviceName = String(settingsManager->getSettings()->mqttDeviceName);
        stopAp();
        startSta(deviceName);
        LOGGER.info("IP " + (WiFi.isConnected() ? WiFi.localIP().toString() : String("0.0.0.0")));
        return;
    }

    // STA keep-alive / recovery (not used while AP is up).
    const bool connected =
            WiFi.isConnected() && (WiFi.localIP() != IPAddress(0, 0, 0, 0));

    if (connected) {
        if (!staWasConnected) {
            onStaConnected();
        }
        staWasConnected = true;
        return;
    }

    if (staWasConnected) {
        LOGGER.warning("STA link lost" + getTextErrorStatus());
        staWasConnected = false;
    }

    if ((millis() - lastStaReconnectMs) < kStaReconnectMs) {
        return;
    }
    lastStaReconnectMs = millis();
    reconnectSta();
}

void WiFiController::init() {
    GlobalSettings *settings = settingsManager->getSettings();
    String deviceName = String(settings->mqttDeviceName);

    LOGGER.info("Configuring WiFi...");

    if (forceAp) {
        startAp(deviceName);
    } else {
        startSta(deviceName);
    }

    LOGGER.info("IP " + (isApMode() ? WiFi.softAPIP() : WiFi.localIP()).toString());
    LOGGER.info("MAC " + WiFi.macAddress());
    LOGGER.info("WiFi current state: " + String(WiFi.getMode()));

    serverController = new WebServerController(settingsManager);
}
