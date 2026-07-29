//
// Created by dsporykhin on 14.02.21.
//

#ifndef BASE_ESP8266_MQTT_MQTTCLIENT_H
#define BASE_ESP8266_MQTT_MQTTCLIENT_H

#include <PubSubClient.h>
#include "GlobalSettings.h"
#include <WiFi.h>

class MqttClient {
private:
    int port;
    String server;
    WiFiClient *espClient;
    PubSubClient *client;
    GlobalSettings *settings;
    long lastReconnectTime;
    long lastCheckTime;
    static void callback(char* topic, byte* payload, unsigned int length);

    void reconnect();

    void checkConnection();

public:

    MqttClient(GlobalSettings *settings);

    void dispatch();
};


#endif //BASE_ESP8266_MQTT_MQTTCLIENT_H
