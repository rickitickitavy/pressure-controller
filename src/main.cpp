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

    UiMode gMode = UiMode::Run;
    PressureSettings gSettings;
    PressureSettings gDraft;
    SettingsFocus gFocus = SettingsFocus::Leak;
    bool gPumpOn = false;
    bool gThresholdsDirty = false;

    unsigned long gLastActivityMs = 0;
    unsigned long gPumpOnSinceMs = 0;
    unsigned long gAwaitingMinSinceMs = 0;
    bool gAwaitingMin = false;
    unsigned long gLastDisplayMs = 0;

    SettingsManager *settingsManager;
    WiFiController *wiFiController;
    MqttClient *mqtt;
    bool gOtaEnabled = false;
    bool gLastWifiConnected = false;

    void setPump(bool on) {
        if (on == gPumpOn) {
            return;
        }
        gPumpOn = on;
        digitalWrite(PIN_PUMP, on ? HIGH : LOW);
        if (on) {
            gPumpOnSinceMs = millis();
            gAwaitingMin = true;
            gAwaitingMinSinceMs = millis();
        } else {
            gAwaitingMin = false;
        }
    }

    void enterFail() {
        setPump(false);
        gMode = UiMode::Fail;
        gThresholdsDirty = false;
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
        // #region agent log
        Serial.printf(
            "{\"sessionId\":\"2ce4f1\",\"hypothesisId\":\"S\",\"location\":\"main.cpp:enterSettings\","
            "\"message\":\"enter_settings\",\"data\":{\"leak\":%u,\"weak\":%u},"
            "\"timestamp\":%lu,\"runId\":\"settings-ui\"}\n",
            static_cast<unsigned>(gDraft.leakDetectSec),
            static_cast<unsigned>(gDraft.pumpWeakSec),
            static_cast<unsigned long>(millis()));
        // #endregion
    }

    void applySettingsSave() {
        gSettings.leakDetectSec = gDraft.leakDetectSec;
        gSettings.pumpWeakSec = gDraft.pumpWeakSec;
        gSettings.sensorMaxMpa = gDraft.sensorMaxMpa;
        SettingsManager::clampAdvanced(gSettings);
        SettingsManager::clampPair(gSettings.minMpa, gSettings.maxMpa, gSettings.sensorMaxMpa);
        settingsManager->savePressure(gSettings);
        gSettings = settingsManager->getSettings()->pressure;
        Pressure::setSensorMaxMpa(gSettings.sensorMaxMpa);
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
                int v = static_cast<int>(gDraft.leakDetectSec) + steps;
                if (v < LEAK_SEC_MIN) {
                    v = LEAK_SEC_MIN;
                }
                if (v > LEAK_SEC_MAX) {
                    v = LEAK_SEC_MAX;
                }
                gDraft.leakDetectSec = static_cast<uint16_t>(v);
                break;
            }
            case SettingsFocus::Weak: {
                int v = static_cast<int>(gDraft.pumpWeakSec) + steps;
                if (v < WEAK_SEC_MIN) {
                    v = WEAK_SEC_MIN;
                }
                if (v > WEAK_SEC_MAX) {
                    v = WEAK_SEC_MAX;
                }
                gDraft.pumpWeakSec = static_cast<uint16_t>(v);
                break;
            }
            case SettingsFocus::SensorMax:
                gDraft.sensorMaxMpa += static_cast<float>(steps) * SENSOR_MAX_STEP_MPA;
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
            case UiMode::Fail:
            case UiMode::Settings:
                break;
        }
    }

    void updateControl(float pressure) {
        if (gMode == UiMode::Fail || gMode == UiMode::Settings) {
            return;
        }

        if (!gPumpOn && pressure < gSettings.minMpa) {
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
            } else if ((millis() - gAwaitingMinSinceMs) >= leakMs) {
                enterFail();
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

    gSettings = settingsManager->getSettings()->pressure;
    gDraft = gSettings;
    Pressure::setSensorMaxMpa(gSettings.sensorMaxMpa);

    Pressure::begin(PIN_PRESSURE);
    Encoder::begin(PIN_ENCODER_A, PIN_ENCODER_B, PIN_ENCODER_BTN);
    Display::begin();

    delay(50);
    const bool forceAp = digitalRead(PIN_ENCODER_BTN) == LOW;
    if (forceAp) {
        LOGGER.info("Encoder button held at boot — WiFi AP mode requested");
    }

    wiFiController = new WiFiController(settingsManager, forceAp);
    LOGGER.info("WiFi controller started");

    if (WiFi.isConnected() || wiFiController->isApMode()) {
        ArduinoOTA.begin();
        gOtaEnabled = true;
        LOGGER.info("OTA started");
    } else {
        LOGGER.info("OTA skipped (WiFi not connected / not AP)");
    }

    if (wiFiController->isApMode()) {
        mqtt = nullptr;
        LOGGER.info("MQTT client skipped (WiFi AP mode)");
    } else if (!WiFi.isConnected()) {
        mqtt = nullptr;
        LOGGER.info("MQTT client skipped (WiFi STA not connected)");
    } else {
        mqtt = new MqttClient(settingsManager->getSettings());
        LOGGER.info("MQTT client started");
    }

    gLastActivityMs = millis();
    Serial.println("Pump controller ready");
}

void loop() {
    Encoder::update();

    wiFiController->update();
    const bool isAp = wiFiController->isApMode();

    // OTA while STA connected or AP is up; restart OTA if AP timed out into STA.
    const bool otaWanted = WiFi.isConnected() || isAp;
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
    if (!isAp && mqtt == nullptr && WiFi.isConnected()) {
        mqtt = new MqttClient(settingsManager->getSettings());
        LOGGER.info("MQTT client started");
    }

    if (mqtt != nullptr) {
        mqtt->dispatch();
    }

    if (gMode == UiMode::Fail) {
        (void) Encoder::consumePress();
        (void) Encoder::consumeHoldPress();
        (void) Encoder::consumeLongPress();
        (void) Encoder::consumeSteps();
    } else if (gMode == UiMode::Settings) {
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
        ui.apMode = wiFiController->isApMode();
        ui.wifiIcon = ui.apMode || wifiConnected;
        ui.draft = gDraft;
        ui.focus = gFocus;

        if (wifiReconnected) {
            Display::invalidateWifi();
        }
        Display::render(ui);
    }
}
