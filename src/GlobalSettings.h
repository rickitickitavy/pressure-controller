#ifndef EFLAME328_GLOBALSETTINGS_H
#define EFLAME328_GLOBALSETTINGS_H

#include <Arduino.h>

#define GLOBAL_CURRENT_SETTINGS_VERSION 2
#define GLOBAL_SETTINGS_MARKER_0 0x30
#define GLOBAL_SETTINGS_MARKER_1 0x32
#define GLOBAL_SETTINGS_MARKER_2 0x33
#define GLOBAL_SETTINGS_MARKER_3 0x37

#define MAX_PROFILE_COUNTER 10
#define SYSTEM_PROFILE_COUNTER 5

constexpr float PRESSURE_ATM_MPA = 0.100f; // 1 Atm = 0.1 MPa (display conversion only)
constexpr float PRESSURE_SENSOR_MAX_DEFAULT_MPA = 0.500f;
constexpr float PRESSURE_STEP_MPA = 0.001f;
constexpr float PRESSURE_MIN_GAP_MPA = 0.002f;
constexpr float SENSOR_MAX_STEP_MPA = 0.010f; // 0.1 atm
constexpr float SENSOR_MAX_MIN_MPA = 0.200f; // 2.0 atm
constexpr float SENSOR_MAX_MAX_MPA = 5.000f; // 50.0 atm

constexpr int LEAK_SEC_MIN = 5;
constexpr int LEAK_SEC_MAX = 40;
constexpr int WEAK_SEC_MIN = 40;
constexpr int WEAK_SEC_MAX = 600;

constexpr float MQTT_PRESSURE_UPDATE_DIFF_MIN_ATM = 0.01f;
constexpr float MQTT_PRESSURE_UPDATE_DIFF_MAX_ATM = 0.5f;
constexpr float MQTT_PRESSURE_UPDATE_DIFF_DEFAULT_ATM = 0.05f;

constexpr int MQTT_PUBLISH_MIN_INTERVAL_SEC_MIN = 5;
constexpr int MQTT_PUBLISH_MIN_INTERVAL_SEC_MAX = 120;
constexpr int MQTT_PUBLISH_MIN_INTERVAL_SEC_DEFAULT = 10;

struct NetworkSettings {
    /**
  * Настройки WiFi
  */
    char ssid[64];
    char password[64];

    /**
     * device name in the network
     */
    char hostName[64];

    /**
     * When true, ArduinoOTA is allowed while STA is connected.
     * AP mode always allows OTA regardless of this flag.
     */
    bool enableOtaOnNetwork = false;
};

struct PressureSettings {
    float minMpa = 0.200f;
    float maxMpa = 0.350f;
    int leakDetectSec = 10; // time to reach min after pump ON
    int pumpWeakSec = 180; // max continuous pump ON
    float sensorMaxMpa = 0.500f; // ADC full-scale pressure
    bool leakDetectEnabled = true; // when false, LEAK timeout is ignored
};

struct GlobalSettings {
    /**
     * Must contain some value tells than the settings was once initialized. If this value doesn't equal the expected then the device was not initialized and default setting must be saved
    */
    char initMarker[4];

    /**
     * Версия настроек
     */
    unsigned char version;

    /**
     * Сетевые настройки и настройки WiFi
     */
    NetworkSettings network;

    /**
     * Mqtt server or IP
     */
    char mqttServer[64];

    /**
     * Mqtt port. default 1883
     */
    int mqttPort;

    /**
     * Reconnect interval for MQTT if it broke
     */
    long mqttReconnectIntervalMs;

    /**
     * Unique, in home, deviceName
     */
    char mqttDeviceName[32];

    /**
     * The name of topic to report the device is alive
     */
    char topicTheDeviceIsAlive[32];

    /**
     * The name of topic to report the pump state
     */
    char topicThePumpState[32];

    /**
     * The name of topic to report the pressure value
     */
    char topicPressureValue[32];

    /**
     * The name of topic to listen for commands
     */
    char topicToListenCommands[32];

    /**
     * The name of topic to listen when server has born again
     */
    char topicToListenServerWasBorn[32];

    /**
     * Pump / pressure controller settings
     */
    PressureSettings pressure;

    /**
     * Publish pressure over MQTT only when |Δatm| exceeds this value (appended; no schema version bump).
     */
    float pressureUpdateDiffAtm = MQTT_PRESSURE_UPDATE_DIFF_DEFAULT_ATM;

    /**
     * Min seconds between change-driven pressure publishes (appended; no schema version bump).
     */
    int pressurePubMinIntSec = MQTT_PUBLISH_MIN_INTERVAL_SEC_DEFAULT;

    /**
     * Min seconds between change-driven pump-state publishes (appended; no schema version bump).
     */
    int pumpStatePubMinIntSec = MQTT_PUBLISH_MIN_INTERVAL_SEC_DEFAULT;
};

#endif //EFLAME328_SETTINGS_H
