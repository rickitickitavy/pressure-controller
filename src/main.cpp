#include <Arduino.h>
#include "adafruit/Adafruit_ST7789.h"
#include "adafruit/FreeMonoBoldOblique24pt7b.h"
#include <avr/eeprom.h>

#define TFT_CS        10
#define TFT_RST        9
#define TFT_DC         8

#define PUMP_PIN 12

#define ADC_V 5.0F
#define ADC_STEPS 1024.0F

unsigned long  lastWork;
uint32_t lastScreenUpdated = 0;
float lastDisplayedValue = 0;
float ema_value = -1;
float real_value = -1;

Adafruit_ST7789 *tft;

struct settings_t{
    char sig = 0x52;
    float max_pressure = 3.2;
    float min_pressure = 1.7;
    float ema = 0.1;
    unsigned long time_to_reach_half_of_pressure = 40000;
    unsigned long max_pump_on_time = 120000;
    unsigned long time_to_reach_min_pressure = 5000;
    unsigned long time_press_for_on_off_pump_ms = 1000;
    float sensor_V = 5.0F;
    unsigned long scan_sensor_sensor_ms = 20;
    float sensor_corr = 1.0;
};

unsigned long button_press_time = 0;
bool button_pressed_status = false;
unsigned long anti_buzzle_ms = 150;
settings_t settings;
bool pump_enabled = true;
bool pump_active = false;
unsigned long pump_started_at = 0;
bool failed = false;
long min_max_changed_at = -1;

int action_code = -1;
int range_index = -1;
long range_index_changed_at = 0;
bool long_press = false;

float v_op;

#define ST77XX_DARKGREEN 0x03E0
#define ST77XX_DARKGRAY 0x31E7
#define ST77XX_DARKGRAY2 0x2104

int16_t pump_x[] = {32, 47, 22, 89, 98, 11, 0, 38};
int16_t pump_w[] = {40, 10, 66, 8, 14, 10, 10, 50};
int16_t pump_y[] = {0, 8, 18, 38, 23, 43, 28, 68};
int16_t pump_h[] = {8, 10, 50, 20, 50, 10, 40, 10};

void drawPump(int16_t x, int16_t y, uint16_t color) {
    for (int index = 0; index < 8; index++)
        tft->fillRect(x + pump_x[index], y + pump_y[index], pump_w[index], pump_h[index], color);

    tft->fillTriangle(x + 22, y + 18, x + 32, y + 18, x + 22, y + 28, ST77XX_BLACK);
    tft->fillTriangle(x + 38, y + 69, x + 47, y + 78, x + 38, y + 78, ST77XX_BLACK);
}

void drawPump(uint16_t color){
    drawPump(50, 140, color);
}

void saveSettings(){
    eeprom_write_block(&settings, 0, sizeof(settings));
}

void loadSettings(){
    if (eeprom_read_byte(0) != settings.sig){
        // eeprom was not initialized
        saveSettings();
    } else {
        eeprom_read_block(&settings, 0, sizeof(settings) );
    }
}

void updateCurrentPressure(){
    tft->setCursor(50, 110);
    tft->fillRect(50, 80, 120, 38, ST77XX_BLACK);
    tft->setTextColor(failed ? ST77XX_RED : ST77XX_WHITE);
    tft->printf("%s", String(lastDisplayedValue).c_str());
}

void drawMinPressure() {
    tft->setTextColor(ST77XX_WHITE);
    tft->fillRect(112, 249, 120, 38, ST77XX_DARKGREEN);
    tft->setCursor(112, 283);
    tft->print(String(settings.min_pressure));
}

void drawMaxPressure(){
    tft->setTextColor(ST77XX_YELLOW);
    tft->fillRect(112, 8, 120, 38, ST77XX_RED);
    tft->setCursor(112, 42);
    tft->print(String(settings.max_pressure));
}

void rotaryDetected(){
    if (range_index >= 0) {
        if (digitalRead(4))
            action_code = 1 + (range_index << 1);
        else
            action_code = 0 + (range_index << 1);
    }
}

int readButton() {
    long time = millis();
    if ((time - button_press_time) > anti_buzzle_ms) {
        // it's time to check button state
        int free = digitalRead(3);

        if (!free && long_press)
            return -1;

        long_press = false;

        if (!button_pressed_status && (!free)) {
            button_pressed_status = true;
            button_press_time = time;
            return 5;
        } else if ((button_pressed_status) && (free)) {
            button_pressed_status = false;
            button_press_time = time;
            return -1;
        } else if (button_pressed_status && !free && ((time - button_press_time) > settings.time_press_for_on_off_pump_ms)) {
            button_press_time = time;
            long_press = true;
            return 4;
        }else
            return -1;
    } else
        return -1;

}

int getCurrentAction() {
    int btn_code = readButton();
    if (btn_code != -1)
        return btn_code;
    else {
        int ret_value = action_code;
        action_code = -1;
        return ret_value;
    }
}


void setup() {
    pinMode(PUMP_PIN, OUTPUT);
    digitalWrite(PUMP_PIN, LOW);

    lastWork = millis();
    loadSettings();

    tft = new Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
    tft->init(240, 320);
    tft->setRotation(2);
    tft->fillScreen(ST77XX_BLACK);

    tft->setFont(&FreeMonoBoldOblique24pt7b);
    tft->setCursor(10, 130);
    tft->setTextColor(ST77XX_YELLOW);
    tft->print("Ver A-1");

    delay(800);
    tft->fillScreen(ST77XX_BLACK);

    tft->fillRect(0, 0, 240, 64, ST77XX_RED);
    tft->setCursor(7, 42);
    tft->print("Max");
    drawMaxPressure();

    tft->fillRect(0, 241, 240, 64, ST77XX_DARKGREEN);
    tft->setCursor(7, 283);
    tft->setTextColor(ST77XX_WHITE);
    tft->print("Min");
    drawMinPressure();

    pinMode(A0, INPUT);

    drawPump(ST77XX_DARKGRAY2);

    v_op = 5.0F / settings.sensor_V;

    pinMode(2, INPUT);
    pinMode(3, INPUT_PULLUP);
    pinMode(4, INPUT);

    attachInterrupt(digitalPinToInterrupt(2), rotaryDetected, FALLING);

}

void drawOffEditMode(){
    switch (range_index) {
        case 0: tft->fillRect(0, 53, 240, 6, ST77XX_RED);
            break;
        case 1: tft->fillRect(0, 293, 240, 6, ST77XX_DARKGREEN);
            break;
    }
}

void manageButtons() {

    long time = millis();
    // check to time to save changes if any
    if (min_max_changed_at != -1){
        if ((time - min_max_changed_at) > 4000){
            // time to save
            min_max_changed_at = -1;
            saveSettings();
            tft->fillCircle(30, 95, 10, ST77XX_BLACK);
            drawOffEditMode();
            range_index = -1;
            range_index_changed_at = -1;
        }
    }

    if (
        (((min_max_changed_at != -1) && ((time - min_max_changed_at) > 4000)) || (min_max_changed_at == -1))
            && (range_index_changed_at != -1) && ((time - range_index_changed_at) > 4000)
      ){

        drawOffEditMode();
        range_index = -1;
        range_index_changed_at = -1;
    }

    bool max_changed = false;
    bool min_changed = false;
    bool pump_enabled_changed = false;

    int button_code = getCurrentAction();//testButtons();
    switch (button_code) {
        case 0:
            if (settings.max_pressure < 3.8F) {
                settings.max_pressure += 0.01;
                max_changed = true;
            }
            break;
        case 1:
            if (settings.max_pressure > 2.001F) {
                settings.max_pressure -= 0.01;
                max_changed = true;

                if (settings.min_pressure > (settings.max_pressure - 0.1)) {
                    settings.min_pressure = settings.max_pressure - 0.1;
                    min_changed = true;
                }
            }
            break;
        case 2:
            if (settings.min_pressure < 2.2F) {
                settings.min_pressure += 0.01;
                min_changed = true;

                if (settings.min_pressure > (settings.max_pressure - 0.1)) {
                    settings.max_pressure = settings.min_pressure + 0.1;
                    max_changed = true;
                }
            }
            break;
        case 3:
            if (settings.min_pressure > 1.201F) {
                settings.min_pressure -= 0.01;
                min_changed = true;
            }
            break;
        case 4:
            drawOffEditMode();
            range_index = -1;
            pump_enabled = !pump_enabled;
            pump_enabled_changed = true;
            if (!pump_enabled) {
                pump_active = false;
                digitalWrite(PUMP_PIN, LOW);
            }
            break;
        case 5:
            drawOffEditMode();
            range_index_changed_at = time;
            range_index++;
            if (range_index > 1)
                range_index = -1;

            switch (range_index) {
                case 0: tft->fillRect(0, 53, 240, 6, ST77XX_YELLOW);
                    break;
                case 1: tft->fillRect(0, 293, 240, 6, ST77XX_WHITE);
                    break;
            }
            break;
        default:
            break;
    }

    if (max_changed)
        drawMaxPressure();

    if (min_changed)
        drawMinPressure();

    if (pump_enabled_changed){
        drawPump(pump_enabled ? ST77XX_DARKGRAY2 : ST77XX_RED);
    }

    if (max_changed || min_changed){
        if (min_max_changed_at == -1)
            tft->fillCircle(30, 95, 10, ST77XX_GREEN);
        min_max_changed_at = time;
    }
}

void  manageActivePump(){
    if (pump_active){
        unsigned long time_spent = millis() - pump_started_at;

        if (((real_value < ((settings.max_pressure - settings.min_pressure) / 2.0F + settings.min_pressure))
            && (time_spent >= settings.time_to_reach_half_of_pressure))

            || (time_spent > settings.max_pump_on_time)

            || ((time_spent > settings.time_to_reach_min_pressure) && real_value < settings.min_pressure)){


            // failure
            digitalWrite(PUMP_PIN, LOW);
            failed = true;
            pump_enabled = false;
            pump_active = false;
            updateCurrentPressure();
            drawPump(ST77XX_RED);

            tft->setCursor(180, 190);
            if ((millis() - pump_started_at) > settings.max_pump_on_time) {
                failed = false;
                updateCurrentPressure();
            }
            else if (real_value < settings.min_pressure)
                tft->print("E0");
            else
                tft->print("E1");
        }
    }
}

void loop() {
    if (!failed) {
        manageButtons();
        manageActivePump();
    }
    if ((millis() - lastWork) > settings.scan_sensor_sensor_ms) {
        lastWork = millis();
        real_value = analogRead(A0) * v_op * (ADC_V / ADC_STEPS) * settings.sensor_corr - 1.0F;
        if (ema_value == -1)
            ema_value = real_value;
        else
            ema_value = ema_value * (1.0F - settings.ema) + real_value * settings.ema;

        // process value
        if (ema_value <= settings.min_pressure){
            if (pump_enabled && !pump_active) {
                // turn the pump on
                digitalWrite(PUMP_PIN, HIGH);
                pump_active = true;
                pump_started_at = millis();
                drawPump(ST77XX_GREEN);
            }
        } else if (ema_value >= settings.max_pressure){
            if (pump_active){
                digitalWrite(PUMP_PIN, LOW);
                pump_active = false;
                pump_started_at = millis();
                drawPump(ST77XX_DARKGRAY2);
            }
        }

        // display
        if ((abs(lastDisplayedValue - real_value) > 0.05)
            || ((abs(lastDisplayedValue - ema_value) >= 0.01) && ((lastWork - lastScreenUpdated) > 500))) {
            lastDisplayedValue = ema_value;
            lastScreenUpdated = lastWork;

            updateCurrentPressure();
        }

    }
}