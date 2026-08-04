#pragma once

// Sensor & actuators
constexpr int PIN_PRESSURE = 0;
constexpr int PIN_ENCODER_A = 10;
constexpr int PIN_ENCODER_B = 1;
constexpr int PIN_ENCODER_BTN = 8;
constexpr int PIN_PUMP = 9;

// ST7789 SPI (no backlight pin)
constexpr int PIN_TFT_MOSI = 6;
constexpr int PIN_TFT_SCLK = 4;
constexpr int PIN_TFT_MISO = 5; // unused; ESP32-C3 SPI HAL rejects MISO=-1
constexpr int PIN_TFT_CS = 7;
constexpr int PIN_TFT_DC = 2;
constexpr int PIN_TFT_RST = 3;

constexpr int TFT_WIDTH = 240;
constexpr int TFT_HEIGHT = 320;
