#include <Arduino.h>
#include "adafruit/Adafruit_GFX.h"    // Core graphics library
#include "adafruit/Adafruit_ST7735.h" // Hardware-specific library for ST7735
#include "adafruit/Adafruit_ST7789.h"
#include "adafruit/FreeSans12pt7b.h"
#include "adafruit/FreeMonoBoldOblique24pt7b.h"
#include "adafruit/FreeSans9pt7b.h"
#include <SPI.h>

#define TFT_CS        10
#define TFT_RST        9
#define TFT_DC         8


uint32_t lastWork;
uint32_t lastScreenUpdated = 0;
float lastDisplayedValue = 0;
float emaValue = -1;
float ema = 0.3;

Adafruit_ST7789 *tft;

enum BUTTON_CODE {
    NONE = 0, MAX_UP, MAX_DOWN, MIN_MAX, MIN_DOWN, LOCK
};

long button_press_time[5] = {0, 0, 0, 0, 0};
bool button_pressed_status[5] = {false, false, false, false, false};
int button_pins[5] = {7, 6, 5, 4, 3};
int button_code = BUTTON_CODE::NONE;
long anti_buzzle_ms = 50;
float max_pressure = 3.2;
float min_pressure = 1.7;

#define ST77XX_DARKGREEN 0x03E0

void setup() {
    Serial.begin(57600);
    delay(250);
    Serial.println("sssppp5");
    lastWork = millis();

    tft = new Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
    tft->init(240, 320);
    tft->setRotation(2);
    tft->fillScreen(ST77XX_BLACK);
    tft->setFont(&FreeSans12pt7b);
    tft->setFont(&FreeSans9pt7b);

    tft->setCursor(10, 307);
    tft->setTextColor(ST77XX_YELLOW);
    tft->print("Pressure controller V 0.99");

    tft->setFont(&FreeMonoBoldOblique24pt7b);
    tft->fillRect(0, 0, 240, 64, ST77XX_RED);
    tft->setCursor(7, 42);
    tft->print("Max");
    tft->setCursor(112, 42);
    tft->print(String(max_pressure));

    tft->fillRect(0, 220, 240, 64, ST77XX_DARKGREEN);
    tft->setCursor(7, 262);
    tft->setTextColor(ST77XX_WHITE);
    tft->print("Min");
    tft->setCursor(112, 262);
    tft->print(String(min_pressure));

    pinMode(A0, INPUT);
    for (int index = 0; index < 5; pinMode(button_pins[index++], INPUT_PULLUP));
}

bool readStatusButton(int index) {
    long time = millis();
    if ((time - button_press_time[index]) > anti_buzzle_ms) {
        // it's time to check button state
        int free = digitalRead(button_pins[index]);
        if (!button_pressed_status[index] && (!free)) {
            button_pressed_status[index] = true;
            button_press_time[index] = time;
            return true;
        } else if ((button_pressed_status[index]) && (free)) {
            button_pressed_status[index] = false;
            button_press_time[index] = time;
            return false;
        } else
            return false;
    }

}

int testButtons() {
    for (int index = 0; index < 5; index++) {
        if (readStatusButton(index))
            return index;
    }
    return -1;
}

void manageButtons() {
    bool max_changed = false;
    bool min_changed = false;

    int button_code = testButtons();
    switch (button_code) {
        case 0:
            if (max_pressure < 3.8F) {
                max_pressure += 0.05;
                max_changed = true;
            }
            break;
        case 1:
            if (max_pressure > 2.001F) {
                max_pressure -= 0.05;
                max_changed = true;

                if (min_pressure > (max_pressure - 0.5)) {
                    min_pressure = max_pressure - 0.5;
                    min_changed = true;
                }
            }
            break;
        case 2:
            if (min_pressure < 2.2F) {
                min_pressure += 0.05;
                min_changed = true;

                if (min_pressure > (max_pressure - 0.5)) {
                    max_pressure = min_pressure + 0.5;
                    max_changed = true;
                }
            }
            break;
        case 3:
            if (min_pressure > 1.201F) {
                min_pressure -= 0.05;
                min_changed = true;
            }
            break;
        default:
            break;
    }

    if (max_changed) {
        tft->setTextColor(ST77XX_YELLOW);
        tft->fillRect(112, 8, 120, 38, ST77XX_RED);
        tft->setCursor(112, 42);
        tft->print(String(max_pressure));
        Serial.println("max + " + String(max_pressure));
    }

    if (min_changed) {
        tft->setTextColor(ST77XX_WHITE);
        tft->fillRect(112, 228, 120, 38, ST77XX_DARKGREEN);
        tft->setCursor(112, 262);
        tft->print(String(min_pressure));
    }

}

void loop() {
    manageButtons();
    if ((millis() - lastWork) > 20) {
        lastWork = millis();
        float val = analogRead(A0) * 25.0F / (1024.0F * 3.3F);
        if (emaValue == -1)
            emaValue = val;
        else
            emaValue = emaValue * (1.0F - ema) + val * ema;

        if ((abs(lastDisplayedValue - val) > 0.05)
            || ((abs(lastDisplayedValue - emaValue) >= 0.01) && ((lastWork - lastScreenUpdated) > 500))) {
            lastDisplayedValue = emaValue;
            lastScreenUpdated = lastWork;

            tft->setCursor(50, 160);
            tft->fillRect(50, 120, 140, 43, ST77XX_BLACK);
            tft->setTextColor(ST77XX_WHITE);
            tft->printf("%s", String(lastDisplayedValue).c_str());
            Serial.println("new " + String(lastDisplayedValue));

        }

    }
}