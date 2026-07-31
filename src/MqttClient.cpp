//
// Created by dsporykhin on 14.02.21.
//

#include "MqttClient.h"
#include "Logger.h"
#include "Defines.h"
#include <math.h>

MqttClient::MqttClient(GlobalSettings *settings) {
    this->settings = settings;

    this->server = String(settings->mqttServer);
    this->port = settings->mqttPort;

    espClient = new WiFiClient();
    client = new PubSubClient(*espClient);
    client->setServer(server.c_str(), port);

    lastReconnectTime = 0;
    lastCheckTime = 0;
}

void MqttClient::setPumpControlHandler(PumpControlFn fn) {
    pumpControlHandler = fn;
}

void MqttClient::setMessageCallback(MessageCallback cb) {
    client->setCallback(cb);
}

bool MqttClient::pressureIntervalElapsed() const {
    const unsigned long intervalMs =
            static_cast<unsigned long>(settings->pressurePubMinIntSec) * 1000UL;
    return lastPressurePublishMs == 0 || (millis() - lastPressurePublishMs) >= intervalMs;
}

bool MqttClient::pumpStateIntervalElapsed() const {
    const unsigned long intervalMs =
            static_cast<unsigned long>(settings->pumpStatePubMinIntSec) * 1000UL;
    return lastPumpStatePublishMs == 0 || (millis() - lastPumpStatePublishMs) >= intervalMs;
}

void MqttClient::checkConnection() {
    if (!WiFi.isConnected()) {
        return;
    }
    if ((lastCheckTime == 0)
        || ((millis() - lastCheckTime) > 100)) {
        lastCheckTime = millis();
        if ((!client->connected()) &&
            ((millis() - lastReconnectTime > settings->mqttReconnectIntervalMs) || (lastReconnectTime == 0))) {
            lastReconnectTime = millis();
            reconnect();
        }
    }
}

void MqttClient::publishAlive() {
    LOGGER.info("       MQTT notifying the server the device is ready to topic '" +
                String(settings->topicTheDeviceIsAlive) + "'");
    client->publish(settings->topicTheDeviceIsAlive, settings->mqttDeviceName);
}

void MqttClient::publishState(bool pumpOn, bool force) {
    if (!client->connected() || topicPumpState.length() == 0) {
        return;
    }
    if (!force && !pumpStateIntervalElapsed()) {
        pumpStatePublishPending = true;
        return;
    }
    const char *payload = pumpOn ? "ON" : "OFF";
    client->publish(topicPumpState.c_str(), payload, true);
    lastPumpStatePublishMs = millis();
    pumpStatePublishPending = false;
}

void MqttClient::publishDeviceEnabled(bool enabled) {
    if (!client->connected() || topicDeviceEnabled.length() == 0) {
        return;
    }
    const char *payload = enabled ? "ON" : "OFF";
    client->publish(topicDeviceEnabled.c_str(), payload, true);
}

void MqttClient::publishPressure(float pressureMpa, bool force) {
    if (!client->connected() || topicPressure.length() == 0) {
        return;
    }
    const float atm = pressureMpa / PRESSURE_ATM_MPA;
    if (!force && hasPublishedPressure) {
        const float diff = fabsf(atm - lastPublishedPressureAtm);
        if (!(diff > settings->pressureUpdateDiffAtm)) {
            return;
        }
        if (!pressureIntervalElapsed()) {
            return;
        }
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", atm);
    client->publish(topicPressure.c_str(), buf, false);
    lastPublishedPressureAtm = atm;
    hasPublishedPressure = true;
    lastPressurePublishMs = millis();
}

void MqttClient::publishBootstrap() {
    publishAlive();
    publishState(pendingPumpOn, true);
    publishDeviceEnabled(pendingDeviceEnabled);
    publishPressure(pendingPressureMpa, true);
    forcePublishBootstrap = false;
}

void MqttClient::reconnect() {
    if (!client->connected()) {
        LOGGER.info("Attempting MQTT connection...");
        String clientId = String(settings->mqttDeviceName) + "/" + String(settings->mqttDeviceName);
        if (client->connect(clientId.c_str())) {
            LOGGER.info("   MQTT connected.");

            topicPumpState = String(settings->topicThePumpState) + "/" + String(settings->mqttDeviceName);
            topicPressure = String(settings->topicPressureValue) + "/" + String(settings->mqttDeviceName);
            topicCommands = String(settings->topicToListenCommands) + "/" + String(settings->mqttDeviceName);
            topicDeviceEnabled =
                    String(settings->topicIsTheDeviceEnabled) + "/" + String(settings->mqttDeviceName);

            publishBootstrap();

            LOGGER.info("       Subscribing to '" + topicCommands + "' ...");
            LOGGER.info(client->subscribe(topicCommands.c_str())
                        ? "         subscribed."
                        : "         NOT subscribed.");

            LOGGER.info("       Subscribing to '" + String(settings->topicToListenServerWasBorn) + "' ...");
            LOGGER.info(client->subscribe(settings->topicToListenServerWasBorn)
                        ? "         subscribed."
                        : "         NOT subscribed.");
        } else {
            LOGGER.error("connection failed, state=" + String(client->state()));
        }
    }
}

void MqttClient::handleMessage(const String &topic, const String &payload) {
    if (topic == topicCommands) {
        String cmd = payload;
        cmd.trim();
        cmd.toLowerCase();
        if (cmd == "on") {
            LOGGER.info("MQTT command: enable");
            if (pumpControlHandler != nullptr) {
                pumpControlHandler(true);
            }
        } else if (cmd == "off") {
            LOGGER.info("MQTT command: disable");
            if (pumpControlHandler != nullptr) {
                pumpControlHandler(false);
            }
        } else {
            LOGGER.warning("MQTT unknown command: '" + payload + "'");
        }
        return;
    }

    if (topic == String(settings->topicToListenServerWasBorn)) {
        String status = payload;
        status.trim();
        status.toLowerCase();
            LOGGER.info("MQTT server was born");
        if (status == "online") {
            LOGGER.info("MQTT server was born (online) — republishing state");
            forcePublishBootstrap = true;
        }
    }
}

void MqttClient::onMessage(char *topic, byte *payload, unsigned int length) {
    char *data = (char *) malloc(length + 1);
    if (data == nullptr) {
        return;
    }
    memcpy(data, payload, length);
    data[length] = 0;
    String text = String(data);
    free(data);

    LOGGER.info("Message arrived");
    LOGGER.info(topic);
    LOGGER.info(text);

    handleMessage(String(topic), text);
}

void MqttClient::notifyPumpState(bool pumpOn) {
    pendingPumpOn = pumpOn;
    publishState(pumpOn, false);
}

void MqttClient::notifyDeviceEnabled(bool enabled) {
    pendingDeviceEnabled = enabled;
    publishDeviceEnabled(enabled);
}

void MqttClient::dispatch(bool pumpOn, float pressureMpa) {
    pendingPumpOn = pumpOn;
    pendingPressureMpa = pressureMpa;

    checkConnection();
    if (!client->connected()) {
        return;
    }
    client->loop();

    if (forcePublishBootstrap) {
        publishBootstrap();
    } else {
        if (pumpStatePublishPending) {
            publishState(pendingPumpOn, false);
        }
        publishPressure(pressureMpa, false);
    }
}
