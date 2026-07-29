//
// Created by dsporykhin on 14.02.21.
//

#include "MqttClient.h"
#include "Logger.h"
#include "Defines.h"

MqttClient::MqttClient(GlobalSettings *settings) {
    this->settings = settings;

    this->server = String(settings->mqttServer);
    this->port = settings->mqttPort;

    espClient = new WiFiClient();
    client = new PubSubClient(*espClient);
    client->setCallback(callback);
    client->setServer(server.c_str(), port);

    lastReconnectTime = 0;
    lastCheckTime = 0;
}

void MqttClient::checkConnection() {
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

void MqttClient::reconnect() {
    if (!client->connected()) {
        LOGGER.info("Attempting MQTT connection...");
        // Attempt to connect
        String deviceInputTopic = String(settings->mqttDeviceName) + "/" + String(settings->mqttDeviceName);
        if (client->connect(deviceInputTopic.c_str())) {

            LOGGER.info("   MQTT connected.");

            String topicToListenCommand = String(settings->topicToListenCommands) + "/" + String(settings->mqttDeviceName);

            // Once connected, publish an announcement...
            LOGGER.info("       MQTT notifying the server the device is ready to topic '" + String(settings->topicTheDeviceIsAlive) + "'");
            client->publish(settings->topicTheDeviceIsAlive, settings->mqttDeviceName);

            // ... and resubscribe
            LOGGER.info("       Subscribing to '" + topicToListenCommand + "' ...");
            LOGGER.info(client->subscribe(topicToListenCommand.c_str())
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

void MqttClient::callback(char *topic, byte *payload, unsigned int length) {
    LOGGER.info("Message arrived");
    LOGGER.info(topic);
    char *data = (char*) malloc(length + 1);
    memcpy(data, payload, length);
    data[length] = 0;
    String text =  String(data);
    free(data);
    LOGGER.info(text);
}

void MqttClient::dispatch() {
    checkConnection();
    if (client->connected()) {
        client->loop();
    }
}