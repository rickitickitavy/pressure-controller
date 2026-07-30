#include "display.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <SPI.h>
#include <cmath>
#include <cstring>

#include "pins.h"
#include "GlobalSettings.h"

namespace {
    SPIClass spiTft(FSPI);
    Adafruit_ST7789 tft(&spiTft, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

    UiState gLast{};
    bool gHasLast = false;
    bool gSettingsLayout = false;

    // Main screen contiguous bands (320px).
    constexpr int kYMax = 0;
    constexpr int kHMax = 52;
    constexpr int kYMac = 57; // between MAX and current (AP mode MAC); 5px gap after MAX
    constexpr int kHMac = 24;
    constexpr int kYCurrent = 81;
    constexpr int kHCurrent = 77;
    constexpr int kYPump = 158;
    constexpr int kHPump = 100;
    constexpr int kYMin = 258;
    constexpr int kHMin = 62;

    constexpr uint16_t kDarkGreen = 0x0400;
    constexpr uint16_t kPumpOffGray = 0x8410;
    constexpr int kPumpIconW = 112;
    constexpr int kPumpIconH = 78;
    // Top of pump band → icon y=159 (12px above previous centered y=171).
    constexpr int kPumpIconPadY = 1;

    // Settings screen: 5 equal rows.
    constexpr int kSettingsRowH = 64;

    static_assert(kYMax + kHMax + 5 == kYMac, "5px gap max/mac");
    static_assert(kYMac + kHMac == kYCurrent, "gap mac/current");
    static_assert(kYCurrent + kHCurrent == kYPump, "gap current/pump");
    static_assert(kYPump + kHPump == kYMin, "gap pump/min");
    static_assert(kYMin + kHMin == TFT_HEIGHT, "min must reach bottom");
    static_assert(kSettingsRowH * 5 == TFT_HEIGHT, "settings rows must fill height");

    const int16_t pump_x[] = {32, 47, 22, 89, 98, 11, 0, 38};
    const int16_t pump_w[] = {40, 10, 66, 8, 14, 10, 10, 50};
    const int16_t pump_y[] = {0, 8, 18, 38, 23, 43, 28, 68};
    const int16_t pump_h[] = {8, 10, 50, 20, 50, 10, 40, 10};

    void drawPump(int16_t x, int16_t y, uint16_t color) {
        for (int index = 0; index < 8; index++) {
            tft.fillRect(x + pump_x[index], y + pump_y[index], pump_w[index], pump_h[index],
                         color);
        }

        tft.fillTriangle(x + 22, y + 18, x + 32, y + 18, x + 22, y + 28, ST77XX_BLACK);
        tft.fillTriangle(x + 38, y + 69, x + 47, y + 78, x + 38, y + 78, ST77XX_BLACK);
    }

    void drawPump(uint16_t color) {
        const int16_t x = static_cast<int16_t>((TFT_WIDTH - kPumpIconW) / 2);
        const int16_t y = static_cast<int16_t>(kYPump + kPumpIconPadY);
        drawPump(x, y, color);
    }

    float toAtm(float mpa) {
        return mpa / PRESSURE_ATM_MPA;
    }

    void drawBar(int y, int h, const char *text, uint16_t fg, uint16_t bg,
                 const GFXfont *font, bool centerH = false) {
        tft.fillRect(0, y, TFT_WIDTH, h, bg);
        tft.setFont(font);
        tft.setTextSize(1);
        tft.setTextColor(fg);

        int16_t x1 = 0;
        int16_t y1 = 0;
        uint16_t tw = 0;
        uint16_t th = 0;
        tft.getTextBounds(text, 0, 0, &x1, &y1, &tw, &th);

        int baseline = y + (h + static_cast<int>(th)) / 2;
        if (baseline > y + h - 2) {
            baseline = y + h - 2;
        }
        if (baseline < y + static_cast<int>(th)) {
            baseline = y + static_cast<int>(th);
        }

        const int x = centerH ? ((TFT_WIDTH - static_cast<int>(tw)) / 2 - x1) : 6;
        tft.setCursor(x, baseline);
        tft.print(text);
        tft.setFont(nullptr);
    }

    void drawMax(const UiState &state) {
        char buf[32];
        snprintf(buf, sizeof(buf), "MAX %.2f", toAtm(state.maxMpa));
        const bool editing = state.mode == UiMode::EditMax;
        const uint16_t fg = editing ? ST77XX_BLACK : ST77XX_YELLOW;
        const uint16_t bg = editing ? ST77XX_YELLOW : ST77XX_RED;
        drawBar(kYMax, kHMax, buf, fg, bg, &FreeSansBold24pt7b);
    }

    void drawCurrent(const UiState &state) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%.2f", toAtm(state.pressureMpa));
        drawBar(kYCurrent, kHCurrent, buf, ST77XX_CYAN, ST77XX_BLACK, &FreeSansBold24pt7b,
                true);
    }

    void drawMac(const UiState &state) {
        if (state.apMode && state.macAddress[0] != '\0') {
            drawBar(kYMac, kHMac, state.macAddress, ST77XX_WHITE, ST77XX_BLACK,
                    &FreeSansBold12pt7b, true);
        } else {
            tft.fillRect(0, kYMac, TFT_WIDTH, kHMac, ST77XX_BLACK);
        }
    }

    // Native 32x32 WiFi glyph (1bpp, 4 bytes/row, MSB left). Drawn 1:1 — no scaling.
    constexpr int kWifiIconSize = 32;
    // FreeSansBold9pt baseline advance ~12–14px; room for AP + OTA under the icon.
    constexpr int kWifiLabelH = 16;
    constexpr int kWifiBlockW = kWifiIconSize;
    constexpr int kWifiBlockH = kWifiIconSize + kWifiLabelH * 2;

    constexpr uint8_t kWifiIconBitmap[128] = {
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x07, 0xE0, 0x00,
        0x00, 0x3F, 0xFC, 0x00,
        0x00, 0xF8, 0x1F, 0x00,
        0x03, 0xC0, 0x03, 0xC0,
        0x07, 0x00, 0x00, 0xE0,
        0x0C, 0x00, 0x00, 0x30,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x1F, 0xF8, 0x00,
        0x00, 0x7F, 0xFE, 0x00,
        0x00, 0xF0, 0x0F, 0x00,
        0x01, 0xC0, 0x03, 0x80,
        0x03, 0x00, 0x00, 0xC0,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x07, 0xE0, 0x00,
        0x00, 0x1F, 0xF8, 0x00,
        0x00, 0x38, 0x1C, 0x00,
        0x00, 0x60, 0x06, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x03, 0xC0, 0x00,
        0x00, 0x07, 0xE0, 0x00,
        0x00, 0x0C, 0x30, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x01, 0x80, 0x00,
        0x00, 0x03, 0xC0, 0x00,
        0x00, 0x03, 0xC0, 0x00,
        0x00, 0x01, 0x80, 0x00,
        0x00, 0x00, 0x00, 0x00,
    };

    void wifiIconOrigin(int16_t &x, int16_t &y) {
        const int16_t pumpX = static_cast<int16_t>((TFT_WIDTH - kPumpIconW) / 2);
        const int16_t pumpY = static_cast<int16_t>(kYPump + kPumpIconPadY);
        x = static_cast<int16_t>(pumpX + kPumpIconW + 4) + 12;
        // Vertically center icon+optional label block with the pump icon.
        y = static_cast<int16_t>(pumpY + (kPumpIconH - kWifiBlockH) / 2);
    }

    void drawWifiIcon(int16_t x, int16_t y, uint16_t color) {
        for (int row = 0; row < kWifiIconSize; ++row) {
            const uint8_t *rowBytes = &kWifiIconBitmap[row * 4];
            for (int col = 0; col < kWifiIconSize; ++col) {
                const uint8_t byte = rowBytes[col >> 3];
                if (byte & (0x80 >> (col & 7))) {
                    tft.drawPixel(x + col, y + row, color);
                }
            }
        }
    }

    void drawWifiStatus(const UiState &state) {
        int16_t x = 0;
        int16_t y = 0;
        wifiIconOrigin(x, y);
        tft.fillRect(x, y, kWifiBlockW, kWifiBlockH, ST77XX_BLACK);
        if (!state.wifiIcon) {
            return;
        }
        drawWifiIcon(x, y, ST77XX_BLUE);

        auto drawCenteredLabel = [&](const char *label, int row) {
            tft.setFont(&FreeSansBold9pt7b);
            tft.setTextSize(1);
            tft.setTextColor(ST77XX_BLUE);
            int16_t x1 = 0;
            int16_t y1 = 0;
            uint16_t tw = 0;
            uint16_t th = 0;
            tft.getTextBounds(label, 0, 0, &x1, &y1, &tw, &th);
            const int16_t baseline =
                    static_cast<int16_t>(y + kWifiIconSize + kWifiLabelH * row + th);
            tft.setCursor(x + (kWifiBlockW - static_cast<int>(tw)) / 2 - x1, baseline);
            tft.print(label);
            tft.setFont(nullptr);
        };

        int labelRow = 0;
        if (state.apMode) {
            drawCenteredLabel("AP", labelRow++);
        }
        if (state.otaActive) {
            drawCenteredLabel("OTA", labelRow);
        }
    }

    void drawFailE(bool show) {
        const int16_t pumpX = static_cast<int16_t>((TFT_WIDTH - kPumpIconW) / 2);
        tft.fillRect(0, kYPump, pumpX, kHPump, ST77XX_BLACK);
        if (!show) {
            return;
        }

        tft.setFont(&FreeSansBold24pt7b);
        tft.setTextSize(1);
        tft.setTextColor(ST77XX_RED);

        int16_t x1 = 0;
        int16_t y1 = 0;
        uint16_t tw = 0;
        uint16_t th = 0;
        tft.getTextBounds("E", 0, 0, &x1, &y1, &tw, &th);

        const int16_t pumpY = static_cast<int16_t>(kYPump + kPumpIconPadY);
        int baseline = pumpY + (kPumpIconH + static_cast<int>(th)) / 2;
        if (baseline > kYPump + kHPump - 2) {
            baseline = kYPump + kHPump - 2;
        }
        if (baseline < kYPump + static_cast<int>(th)) {
            baseline = kYPump + static_cast<int>(th);
        }

        const int x = (pumpX - static_cast<int>(tw)) / 2 - x1;
        tft.setCursor(x, baseline);
        tft.print("E");
        tft.setFont(nullptr);
    }

    void drawPump(const UiState &state) {
        uint16_t color = kPumpOffGray;
        if (!state.pumpControlEnabled) {
            color = ST77XX_RED;
        } else if (state.pumpOn) {
            color = ST77XX_GREEN;
        }
        drawPump(color);
        drawFailE(state.leakFail);
    }

    void drawMin(const UiState &state) {
        char buf[32];
        snprintf(buf, sizeof(buf), "MIN %.2f", toAtm(state.minMpa));
        const bool editing = state.mode == UiMode::EditMin;
        const uint16_t fg = editing ? ST77XX_BLACK : ST77XX_WHITE;
        const uint16_t bg = editing ? ST77XX_YELLOW : kDarkGreen;
        drawBar(kYMin, kHMin, buf, fg, bg, &FreeSansBold24pt7b);
    }

    void drawSettingsRow(int index, const char *text, bool selected) {
        const int y = index * kSettingsRowH;
        const uint16_t fg = selected ? ST77XX_BLACK : ST77XX_WHITE;
        const uint16_t bg = selected ? ST77XX_YELLOW : ST77XX_BLACK;
        drawBar(y, kSettingsRowH, text, fg, bg, &FreeSansBold12pt7b);
    }

    void drawSettings(const UiState &state) {
        char buf[40];
        snprintf(buf, sizeof(buf), "LEAK %us", state.draft.leakDetectSec);
        drawSettingsRow(0, buf, state.focus == SettingsFocus::Leak);

        snprintf(buf, sizeof(buf), "WEAK %us", state.draft.pumpWeakSec);
        drawSettingsRow(1, buf, state.focus == SettingsFocus::Weak);

        snprintf(buf, sizeof(buf), "SENS %.1f", toAtm(state.draft.sensorMaxMpa));
        drawSettingsRow(2, buf, state.focus == SettingsFocus::SensorMax);

        drawSettingsRow(3, "SAVE", state.focus == SettingsFocus::Save);
        drawSettingsRow(4, "CANCEL", state.focus == SettingsFocus::Cancel);
    }

    bool nearlyEq(float a, float b, float eps) {
        return fabsf(a - b) < eps;
    }

    bool draftChanged(const PressureSettings &a, const PressureSettings &b) {
        return a.leakDetectSec != b.leakDetectSec || a.pumpWeakSec != b.pumpWeakSec ||
               !nearlyEq(a.sensorMaxMpa, b.sensorMaxMpa, 0.001f);
    }
} // namespace

namespace Display {
    void begin() {
        spiTft.begin(PIN_TFT_SCLK, PIN_TFT_MISO, PIN_TFT_MOSI, PIN_TFT_CS);
        tft.init(TFT_WIDTH, TFT_HEIGHT);
        // #region agent log
        constexpr uint8_t kRotation = 2;
        tft.setRotation(kRotation);
        // #endregion
        tft.fillScreen(ST77XX_BLACK);
        gHasLast = false;
        gSettingsLayout = false;
    }

    void render(const UiState &state) {
        if (state.mode == UiMode::Settings) {
            if (!gSettingsLayout) {
                tft.fillScreen(ST77XX_BLACK);
                gSettingsLayout = true;
                gHasLast = false;
            }
            const bool need =
                    !gHasLast || gLast.focus != state.focus || draftChanged(gLast.draft, state.draft) ||
                    gLast.mode != state.mode;
            if (need) {
                drawSettings(state);
                gLast = state;
                gHasLast = true;
            }
            return;
        }

        // Leaving settings layout
        if (gSettingsLayout) {
            tft.fillScreen(ST77XX_BLACK);
            gSettingsLayout = false;
            gHasLast = false;
        }

        const bool first = !gHasLast;
        const bool hlMax = state.mode == UiMode::EditMax;
        const bool hlMin = state.mode == UiMode::EditMin;
        const bool wasHlMax = gLast.mode == UiMode::EditMax;
        const bool wasHlMin = gLast.mode == UiMode::EditMin;

        const bool redrawMax =
                first || !nearlyEq(gLast.maxMpa, state.maxMpa, 0.0005f) || hlMax != wasHlMax;
        const bool redrawMac =
                first || gLast.apMode != state.apMode ||
                strcmp(gLast.macAddress, state.macAddress) != 0;
        const bool redrawCur =
                first || !nearlyEq(gLast.pressureMpa, state.pressureMpa, 0.001f);
        const bool redrawPump =
                first || gLast.pumpOn != state.pumpOn ||
                gLast.pumpControlEnabled != state.pumpControlEnabled ||
                gLast.leakFail != state.leakFail;
        const bool redrawWifi =
                first || gLast.wifiIcon != state.wifiIcon || gLast.apMode != state.apMode ||
                gLast.otaActive != state.otaActive;
        const bool redrawMin =
                first || !nearlyEq(gLast.minMpa, state.minMpa, 0.0005f) || hlMin != wasHlMin;

        if (!redrawMax && !redrawMac && !redrawCur && !redrawPump && !redrawWifi && !redrawMin) {
            return;
        }

        if (redrawMax) {
            drawMax(state);
        }
        if (redrawMac) {
            drawMac(state);
        }
        if (redrawCur) {
            drawCurrent(state);
        }
        if (redrawPump) {
            drawPump(state);
        }
        if (redrawWifi) {
            drawWifiStatus(state);
        }
        if (redrawMin) {
            drawMin(state);
        }

        gLast = state;
        gHasLast = true;
    }

    void invalidateWifi() {
        gLast.wifiIcon = false;
    }
} // namespace Display
