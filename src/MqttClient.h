//
// Created by dsporykhin on 14.02.21.
//

#ifndef BASE_ESP8266_MQTT_MQTTCLIENT_H
#define BASE_ESP8266_MQTT_MQTTCLIENT_H

#include <PubSubClient.h>
#include "GlobalSettings.h"
#include <WiFi.h>

class MqttClient {
public:
    using PumpControlFn = void (*)(bool enabled);
    using MessageCallback = void (*)(char *topic, byte *payload, unsigned int length);

private:
    int port;
    String server;
    WiFiClient *espClient;
    PubSubClient *client;
    GlobalSettings *settings;
    long lastReconnectTime;
    long lastCheckTime;

    PumpControlFn pumpControlHandler = nullptr;

    String topicPumpState;
    String topicPressure;
    String topicCommands;
    String topicDeviceEnabled;

    float lastPublishedPressureAtm = 0.0f;
    bool hasPublishedPressure = false;
    bool forcePublishBootstrap = false;

    unsigned long lastPressurePublishMs = 0;
    unsigned long lastPumpStatePublishMs = 0;
    bool pumpStatePublishPending = false;

    // Pending runtime snapshot for bootstrap publish from callback / reconnect.
    bool pendingPumpOn = false;
    bool pendingDeviceEnabled = true;
    float pendingPressureMpa = 0.0f;

    void reconnect();
    void checkConnection();
    void handleMessage(const String &topic, const String &payload);
    void publishAlive();
    void publishState(bool pumpOn, bool force);
    void publishPressure(float pressureMpa, bool force);
    void publishDeviceEnabled(bool enabled);
    void publishBootstrap();
    bool pressureIntervalElapsed() const;
    bool pumpStateIntervalElapsed() const;

public:
    MqttClient(GlobalSettings *settings);
    ~MqttClient();

    void setPumpControlHandler(PumpControlFn fn);
    void setMessageCallback(MessageCallback cb);

    /** Forward PubSubClient C-callback payloads into command / rebirth handling. */
    void onMessage(char *topic, byte *payload, unsigned int length);

    /** Publish pump ON/OFF when the relay state actually changes (not from dispatch polling). */
    void notifyPumpState(bool pumpOn);

    /** Publish device enabled ON/OFF (MQTT enable/disable, LEAK, boot). */
    void notifyDeviceEnabled(bool enabled);

    void dispatch(bool pumpOn, float pressureMpa);
};


#endif //BASE_ESP8266_MQTT_MQTTCLIENT_H
