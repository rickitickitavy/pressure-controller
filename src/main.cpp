#include <Arduino.h>
#include <ArduinoOTA.h>

#include "display.h"
#include "encoder.h"
#include "pins.h"
#include "pressure.h"
#include <LittleFS.h>
#include "SettingsManager.h"
#include "WiFiController.h"
#include "MqttClient.h"

namespace {
    constexpr unsigned long kEditIdleMs = 5000;
    constexpr unsigned long kSettingsIdleMs = 10000;
    constexpr unsigned long kDisplayMs = 100;
    constexpr unsigned long kWifiRssiAvgMs = 2000;
    constexpr unsigned long kPumpMinToggleMs = 1000;

    UiMode gMode = UiMode::Run;
    PressureSettings gSettings;
    PressureSettings gDraft;
    SettingsFocus gFocus = SettingsFocus::Leak;
    bool gPumpOn = false;
    bool gThresholdsDirty = false;

    unsigned long gLastActivityMs = 0;
    unsigned long gPumpOnSinceMs = 0;
    unsigned long gLastPumpToggleMs = 0;
    unsigned long gAwaitingMinSinceMs = 0;
    bool gAwaitingMin = false;
    unsigned long gLastDisplayMs = 0;

    SettingsManager *settingsManager;
    WiFiController *wiFiController;
    MqttClient *mqtt;
    bool gOtaEnabled = false;
    bool gLastWifiConnected = false;
    bool gPumpControlEnabled = true;
    bool gLeakFail = false;

    int8_t gWifiRssiPercent = -1;
    long gWifiRssiSum = 0;
    unsigned gWifiRssiSamples = 0;
    unsigned long gWifiRssiWindowMs = 0;

    int8_t rssiToPercent(int32_t rssi) {
        if (rssi <= -100) {
            return 0;
        }
        if (rssi >= -50) {
            return 100;
        }
        return static_cast<int8_t>(2 * (rssi + 100));
    }

    void resetWifiRssiAvg() {
        gWifiRssiPercent = -1;
        gWifiRssiSum = 0;
        gWifiRssiSamples = 0;
        gWifiRssiWindowMs = 0;
    }

    void sampleWifiRssi(unsigned long now) {
        gWifiRssiSum += WiFi.RSSI();
        ++gWifiRssiSamples;
        if (gWifiRssiWindowMs == 0) {
            gWifiRssiWindowMs = now;
            return;
        }
        if ((now - gWifiRssiWindowMs) < kWifiRssiAvgMs || gWifiRssiSamples == 0) {
            return;
        }
        const int32_t avgRssi =
                static_cast<int32_t>(gWifiRssiSum / static_cast<long>(gWifiRssiSamples));
        gWifiRssiPercent = rssiToPercent(avgRssi);
        gWifiRssiSum = 0;
        gWifiRssiSamples = 0;
        gWifiRssiWindowMs = now;
    }

    void setPump(bool on) {
        if (on == gPumpOn) {
            return;
        }
        const unsigned long now = millis();
        if (gLastPumpToggleMs != 0 && (now - gLastPumpToggleMs) < kPumpMinToggleMs) {
            return;
        }
        gPumpOn = on;
        digitalWrite(PIN_PUMP, on ? HIGH : LOW);
        gLastPumpToggleMs = now;
        if (on) {
            gPumpOnSinceMs = now;
            gAwaitingMin = true;
            gAwaitingMinSinceMs = now;
        } else {
            gAwaitingMin = false;
        }
        if (mqtt != nullptr) {
            mqtt->notifyPumpState(on);
        }
    }

    void setPumpControlEnabled(bool enabled) {
        gPumpControlEnabled = enabled;
        if (!enabled) {
            setPump(false);
        } else {
            gLeakFail = false;
        }
        if (mqtt != nullptr) {
            mqtt->notifyDeviceEnabled(enabled);
        }
    }

    void leaveEditToRun() {
        if (gThresholdsDirty) {
            settingsManager->savePressure(gSettings);
            gThresholdsDirty = false;
        }
        gMode = UiMode::Run;
    }

    void enterSettings() {
        if (gThresholdsDirty) {
            settingsManager->savePressure(gSettings);
            gThresholdsDirty = false;
        }
        gDraft = gSettings;
        gFocus = SettingsFocus::Leak;
        gMode = UiMode::Settings;
        gLastActivityMs = millis();
    }

    void applySettingsSave() {
        gSettings.leakDetectSec = gDraft.leakDetectSec;
        gSettings.pumpWeakSec = gDraft.pumpWeakSec;
        gSettings.sensorMaxMpa = gDraft.sensorMaxMpa;
        gSettings.sensorMinVolts = gDraft.sensorMinVolts;
        gSettings.sensorMaxVolts = gDraft.sensorMaxVolts;
        SettingsManager::clampAdvanced(gSettings);
        SettingsManager::clampPair(gSettings.minMpa, gSettings.maxMpa, gSettings.sensorMaxMpa);
        settingsManager->savePressure(gSettings);
        gSettings = settingsManager->getSettings()->pressure;
        Pressure::setSensorMaxMpa(gSettings.sensorMaxMpa);
        Pressure::setSensorVolts(gSettings.sensorMinVolts, gSettings.sensorMaxVolts);
        gDraft = gSettings;
        gMode = UiMode::Run;
        gLastActivityMs = millis();
    }

    void applySettingsCancel() {
        gDraft = gSettings;
        gMode = UiMode::Run;
        gLastActivityMs = millis();
    }

    void applyEncoderEdit(int steps) {
        if (steps == 0) {
            return;
        }
        gLastActivityMs = millis();
        const float delta = static_cast<float>(steps) * PRESSURE_STEP_MPA;
        if (gMode == UiMode::EditMax) {
            gSettings.maxMpa += delta;
        } else if (gMode == UiMode::EditMin) {
            gSettings.minMpa += delta;
        } else {
            return;
        }
        SettingsManager::clampPair(gSettings.minMpa, gSettings.maxMpa, gSettings.sensorMaxMpa);
        gThresholdsDirty = true;
    }

    void applySettingsEncoder(int steps) {
        if (steps == 0) {
            return;
        }
        gLastActivityMs = millis();

        switch (gFocus) {
            case SettingsFocus::Leak: {
                int v = gDraft.leakDetectSec + steps;
                if (v < LEAK_SEC_MIN) {
                    v = LEAK_SEC_MIN;
                }
                if (v > LEAK_SEC_MAX) {
                    v = LEAK_SEC_MAX;
                }
                gDraft.leakDetectSec = v;
                break;
            }
            case SettingsFocus::Weak: {
                int v = gDraft.pumpWeakSec + steps;
                if (v < WEAK_SEC_MIN) {
                    v = WEAK_SEC_MIN;
                }
                if (v > WEAK_SEC_MAX) {
                    v = WEAK_SEC_MAX;
                }
                gDraft.pumpWeakSec = v;
                break;
            }
            case SettingsFocus::SensorMax:
                gDraft.sensorMaxMpa += static_cast<float>(steps) * SENSOR_MAX_STEP_MPA;
                SettingsManager::clampAdvanced(gDraft);
                break;
            case SettingsFocus::SensorMinVolts:
                gDraft.sensorMinVolts += static_cast<float>(steps) * SENSOR_VOLT_STEP;
                SettingsManager::clampAdvanced(gDraft);
                break;
            case SettingsFocus::SensorMaxVolts:
                gDraft.sensorMaxVolts += static_cast<float>(steps) * SENSOR_VOLT_STEP;
                SettingsManager::clampAdvanced(gDraft);
                break;
            case SettingsFocus::Save:
            case SettingsFocus::Cancel:
            case SettingsFocus::Count:
                // Buttons are navigated by short press only.
                break;
        }
    }

    void handleSettingsButton() {
        // 1s hold activates SAVE / CANCEL when selected.
        if (Encoder::consumeHoldPress()) {
            gLastActivityMs = millis();
            if (gFocus == SettingsFocus::Save) {
                applySettingsSave();
                return;
            }
            if (gFocus == SettingsFocus::Cancel) {
                applySettingsCancel();
                return;
            }
        }

        // Short press rolls over parameters and buttons.
        if (!Encoder::consumePress()) {
            return;
        }
        gLastActivityMs = millis();
        const auto next = static_cast<uint8_t>(gFocus) + 1;
        if (next >= static_cast<uint8_t>(SettingsFocus::Count)) {
            gFocus = SettingsFocus::Leak;
        } else {
            gFocus = static_cast<SettingsFocus>(next);
        }
    }

    void handleMainButton() {
        if (Encoder::consumeLongPress()) {
            if (gMode == UiMode::Run || gMode == UiMode::EditMax || gMode == UiMode::EditMin) {
                enterSettings();
            }
            (void) Encoder::consumePress(); // discard any pending short
            return;
        }

        if (!Encoder::consumePress()) {
            return;
        }
        gLastActivityMs = millis();

        switch (gMode) {
            case UiMode::Run:
                gMode = UiMode::EditMax;
                break;
            case UiMode::EditMax:
                gMode = UiMode::EditMin;
                break;
            case UiMode::EditMin:
                leaveEditToRun();
                break;
            case UiMode::Settings:
                break;
        }
    }

    void updateControl(float pressure) {
        if (gMode == UiMode::Settings) {
            return;
        }

        if (!gPumpOn && gPumpControlEnabled && pressure < gSettings.minMpa) {
            setPump(true);
        } else if (gPumpOn && pressure >= gSettings.maxMpa) {
            setPump(false);
        }

        if (!gPumpOn) {
            return;
        }

        const unsigned long leakMs =
                static_cast<unsigned long>(gSettings.leakDetectSec) * 1000UL;
        const unsigned long weakMs =
                static_cast<unsigned long>(gSettings.pumpWeakSec) * 1000UL;

        if (gAwaitingMin) {
            if (pressure >= gSettings.minMpa) {
                gAwaitingMin = false;
            } else if (gSettings.leakDetectEnabled &&
                       (millis() - gAwaitingMinSinceMs) >= leakMs) {
                gLeakFail = true;
                setPumpControlEnabled(false);
                return;
            }
        }

        if ((millis() - gPumpOnSinceMs) >= weakMs) {
            setPump(false);
        }
    }

    void updateTimeouts() {
        if (gMode == UiMode::EditMax || gMode == UiMode::EditMin) {
            if ((millis() - gLastActivityMs) >= kEditIdleMs) {
                leaveEditToRun();
            }
        } else if (gMode == UiMode::Settings) {
            if ((millis() - gLastActivityMs) >= kSettingsIdleMs) {
                // #region agent log
                Serial.printf(
                    "{\"sessionId\":\"2ce4f1\",\"hypothesisId\":\"S\",\"location\":\"main.cpp:timeout\","
                    "\"message\":\"settings_idle_cancel\",\"timestamp\":%lu,\"runId\":\"settings-ui\"}\n",
                    static_cast<unsigned long>(millis()));
                // #endregion
                applySettingsCancel();
            }
        }
    }
    void onMqttPumpControl(bool enabled) {
        setPumpControlEnabled(enabled);
    }

    void onMqttMessage(char *topic, byte *payload, unsigned int length) {
        if (mqtt != nullptr) {
            mqtt->onMessage(topic, payload, length);
        }
    }

    void startMqttIfNeeded() {
        if (mqtt != nullptr) {
            return;
        }
        mqtt = new MqttClient(settingsManager->getSettings());
        mqtt->setMessageCallback(onMqttMessage);
        mqtt->setPumpControlHandler(onMqttPumpControl);
        mqtt->notifyDeviceEnabled(gPumpControlEnabled);
        LOGGER.info("MQTT client started");
    }
} // namespace

void setup() {
#ifdef CON_DEBUG
    delay(500);

    Serial.begin(115200);
    Serial.println("--- Starting ");

    LOGGER.info("Started UART at 115200");
#endif
    LOGGER.info("Starting...");

    pinMode(PIN_PUMP, OUTPUT);
    digitalWrite(PIN_PUMP, LOW);

    if (!LittleFS.begin(false)) {
        LOGGER.error("LittleFS mount failed");
    } else {
        LOGGER.info("LittleFS mounted");
    }

    settingsManager = new SettingsManager();
    LOGGER.info("Setting manager started");
    // First-boot defaults: restart immediately from setup (not from EEPROM ctor path).
    if (settingsManager->handlePendingRestart(0)) {
        return;
    }

    gSettings = settingsManager->getSettings()->pressure;
    gDraft = gSettings;
    Pressure::setSensorMaxMpa(gSettings.sensorMaxMpa);
    Pressure::setSensorVolts(gSettings.sensorMinVolts, gSettings.sensorMaxVolts);

    Pressure::begin(PIN_PRESSURE);
    Encoder::begin(PIN_ENCODER_A, PIN_ENCODER_B, PIN_ENCODER_BTN);
    Display::begin(settingsManager->getSettings()->displayRotate180);

    delay(50);
    const bool forceAp = digitalRead(PIN_ENCODER_BTN) == LOW;
    if (forceAp) {
        LOGGER.info("Encoder button held at boot — WiFi AP mode requested");
    }

    wiFiController = new WiFiController(settingsManager, forceAp);
    LOGGER.info("WiFi controller started");

    const bool otaWantedAtBoot =
            wiFiController->isApMode() ||
            (WiFi.isConnected() && settingsManager->getSettings()->network.enableOtaOnNetwork);
    if (otaWantedAtBoot) {
        ArduinoOTA.begin();
        gOtaEnabled = true;
        LOGGER.info("OTA started");
    } else {
        LOGGER.info("OTA skipped (WiFi not connected / not AP / STA OTA disabled)");
    }

    if (wiFiController->isApMode()) {
        mqtt = nullptr;
        LOGGER.info("MQTT client skipped (WiFi AP mode)");
    } else if (!WiFi.isConnected()) {
        mqtt = nullptr;
        LOGGER.info("MQTT client skipped (WiFi STA not connected)");
    } else {
        startMqttIfNeeded();
    }

    gLastActivityMs = millis();
    Serial.println("Pump controller ready");
}

void loop() {
    // WiFi settings save schedules restart; wait so AsyncWebServer can flush HTTP OK.
    if (settingsManager->handlePendingRestart(00)) {
        return;
    }

    Encoder::update();

    wiFiController->update();
    const bool isAp = wiFiController->isApMode();

    // OTA always in AP; in STA only when enableOtaOnNetwork is set.
    const bool otaWanted =
            isAp || (WiFi.isConnected() && settingsManager->getSettings()->network.enableOtaOnNetwork);
    if (otaWanted && !gOtaEnabled) {
        ArduinoOTA.begin();
        gOtaEnabled = true;
        LOGGER.info("OTA started");
    } else if (!otaWanted && gOtaEnabled) {
        gOtaEnabled = false;
        LOGGER.info("OTA stopped");
    }
    if (gOtaEnabled) {
        ArduinoOTA.handle();
    }

    // Start MQTT once STA has a link (boot miss, AP timeout, or later reconnect).
    if (!isAp && WiFi.isConnected()) {
        startMqttIfNeeded();
    }

    if (gMode == UiMode::Settings) {
        (void) Encoder::consumeLongPress();
        handleSettingsButton();
        applySettingsEncoder(Encoder::consumeSteps());
        updateTimeouts();
    } else {
        handleMainButton();
        (void) Encoder::consumeHoldPress(); // unused on main screen
        if (gMode == UiMode::EditMax || gMode == UiMode::EditMin) {
            applyEncoderEdit(Encoder::consumeSteps());
            updateTimeouts();
        } else {
            (void) Encoder::consumeSteps();
        }
    }

    const float pressure = Pressure::readMpa();
    updateControl(pressure);

    if (mqtt != nullptr) {
        mqtt->dispatch(gPumpOn, pressure);
    }

    const unsigned long now = millis();
    if ((now - gLastDisplayMs) >= kDisplayMs) {
        gLastDisplayMs = now;

        const bool wifiConnected = WiFi.isConnected();
        const bool wifiReconnected = wifiConnected && !gLastWifiConnected;
        gLastWifiConnected = wifiConnected;

        UiState ui;
        ui.mode = gMode;
        ui.pressureMpa = pressure;
        ui.minMpa = gSettings.minMpa;
        ui.maxMpa = gSettings.maxMpa;
        ui.pumpOn = gPumpOn;
        ui.pumpControlEnabled = gPumpControlEnabled;
        ui.leakFail = gLeakFail;
        ui.apMode = wiFiController->isApMode();
        ui.wifiIcon = ui.apMode || wifiConnected;
        ui.otaActive = gOtaEnabled;
        if (ui.wifiIcon) {
            sampleWifiRssi(now);
            ui.wifiRssiPercent = gWifiRssiPercent;
        } else {
            resetWifiRssiAvg();
            ui.wifiRssiPercent = -1;
        }
        ui.macAddress[0] = '\0';
        if (ui.apMode) {
            WiFi.macAddress().toCharArray(ui.macAddress, sizeof(ui.macAddress));
        }
        ui.draft = gDraft;
        ui.focus = gFocus;

        if (wifiReconnected) {
            Display::invalidateWifi();
        }
        Display::render(ui);
    }
}
