#ifndef EFLAME328_GLOBALSETTINGS_H
#define EFLAME328_GLOBALSETTINGS_H

#define GLOBAL_CURRENT_SETTINGS_VERSION 1
#define GLOBAL_SETTINGS_MARKER_0 0x32
#define GLOBAL_SETTINGS_MARKER_1 0x32
#define GLOBAL_SETTINGS_MARKER_2 0x33
#define GLOBAL_SETTINGS_MARKER_3 0x37

#define MAX_PROFILE_COUNTER 10
#define SYSTEM_PROFILE_COUNTER 5


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
     * The name of topic to report the device state
     */
    char topicTheDeviceState[32];

    /**
     * The name of topic to listen for commands
     */
    char topicToListenCommands[32];

    /**
     * The name of topic to listen when server has born again
     */
    char topicToListenServerWasBorn[32];

    /**
     * Brightness (0.01 .. 100)
     */
    float bright;

};

#endif //EFLAME328_SETTINGS_H

