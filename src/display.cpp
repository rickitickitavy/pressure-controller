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
    // tft-ui Confirm / Cancel palette (RGB565).
    constexpr uint16_t kConfirmFg = 0xCFF7;
    constexpr uint16_t kConfirmBg = 0x0320;
    constexpr uint16_t kCancelFg = 0xFCD3;
    constexpr uint16_t kCancelBg = 0x9800;
    constexpr uint16_t kCheckMarkGreen = 0x07E0; // tft-ui bold green bird ✓
    constexpr int kCheckStrokePx = 4;
    constexpr int kPumpIconW = 112;
    constexpr int kPumpIconH = 78;
    // Top of pump band → icon y=159 (12px above previous centered y=171).
    constexpr int kPumpIconPadY = 1;

    // Settings screen: rows cover full height (no leftover strip / no fillScreen).
    constexpr int kSettingsRowCount = static_cast<int>(SettingsFocus::Count);
    constexpr int kActionBtnPadX = 10;
    constexpr int kActionBtnPadY = 2;
    constexpr int kCheckSize = 14;
    constexpr int kCheckRightPad = 8;
    constexpr int kLabelPadX = 6;

    static_assert(kYMax + kHMax + 5 == kYMac, "5px gap max/mac");
    static_assert(kYMac + kHMac == kYCurrent, "gap mac/current");
    static_assert(kYCurrent + kHCurrent == kYPump, "gap current/pump");
    static_assert(kYPump + kHPump == kYMin, "gap pump/min");
    static_assert(kYMin + kHMin == TFT_HEIGHT, "min must reach bottom");
    static_assert(kSettingsRowCount == 12, "settings focus enum drives row count");

    int settingsRowY(int index) {
        return (index * TFT_HEIGHT) / kSettingsRowCount;
    }

    int settingsRowH(int index) {
        return settingsRowY(index + 1) - settingsRowY(index);
    }

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
    // FreeSansBold9pt baseline advance ~12–14px; room for % + AP + OTA under the icon.
    constexpr int kWifiLabelH = 16;
    // Wider than icon so "100%" / "OTA" clear without clipping (≤48px to screen edge).
    constexpr int kWifiBlockW = 48;
    constexpr int kWifiBlockH = kWifiIconSize + kWifiLabelH * 3;

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
        // Icon is 32px; center it horizontally in the wider status block.
        drawWifiIcon(static_cast<int16_t>(x + (kWifiBlockW - kWifiIconSize) / 2), y, ST77XX_BLUE);

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
        if (state.wifiRssiPercent >= 0) {
            char pct[8];
            snprintf(pct, sizeof(pct), "%d%%", static_cast<int>(state.wifiRssiPercent));
            drawCenteredLabel(pct, labelRow++);
        }
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

    bool nearlyEq(float a, float b, float eps) {
        return fabsf(a - b) < eps;
    }

    void drawSettingsRow(int index, const char *name, const char *value, bool selected,
                         bool editing, uint16_t idleBg = ST77XX_BLACK) {
        const int y = settingsRowY(index);
        const int h = settingsRowH(index);
        uint16_t fg = ST77XX_WHITE;
        uint16_t bg = idleBg;
        if (selected) {
            fg = ST77XX_BLACK;
            bg = editing ? ST77XX_CYAN : ST77XX_YELLOW;
        }
        tft.fillRect(0, y, TFT_WIDTH, h, bg);

        tft.setFont(&FreeSansBold12pt7b);
        tft.setTextSize(1);
        tft.setTextColor(fg);
        int16_t x1 = 0;
        int16_t y1 = 0;
        uint16_t tw = 0;
        uint16_t th = 0;
        tft.getTextBounds(name, 0, 0, &x1, &y1, &tw, &th);
        int baseline = y + (h + static_cast<int>(th)) / 2;
        if (baseline > y + h - 2) {
            baseline = y + h - 2;
        }
        if (baseline < y + static_cast<int>(th)) {
            baseline = y + static_cast<int>(th);
        }
        tft.setCursor(kLabelPadX - x1, baseline);
        tft.print(name);

        if (value != nullptr && value[0] != '\0') {
            tft.getTextBounds(value, 0, 0, &x1, &y1, &tw, &th);
            tft.setCursor(TFT_WIDTH - kLabelPadX - static_cast<int>(tw) - x1, baseline);
            tft.print(value);
        }
        tft.setFont(nullptr);
    }

    void drawSettingsCheckboxRow(int index, const char *label, bool checked, bool selected,
                                 bool editing) {
        const int y = settingsRowY(index);
        const int h = settingsRowH(index);
        uint16_t fg = ST77XX_WHITE;
        uint16_t bg = ST77XX_BLACK;
        if (selected) {
            fg = ST77XX_BLACK;
            bg = editing ? ST77XX_CYAN : ST77XX_YELLOW;
        }
        tft.fillRect(0, y, TFT_WIDTH, h, bg);

        tft.setFont(&FreeSansBold12pt7b);
        tft.setTextSize(1);
        tft.setTextColor(fg);
        int16_t x1 = 0;
        int16_t y1 = 0;
        uint16_t tw = 0;
        uint16_t th = 0;
        tft.getTextBounds(label, 0, 0, &x1, &y1, &tw, &th);
        int baseline = y + (h + static_cast<int>(th)) / 2;
        if (baseline > y + h - 2) {
            baseline = y + h - 2;
        }
        if (baseline < y + static_cast<int>(th)) {
            baseline = y + static_cast<int>(th);
        }
        tft.setCursor(kLabelPadX - x1, baseline);
        tft.print(label);
        tft.setFont(nullptr);

        const int boxX = TFT_WIDTH - kCheckRightPad - kCheckSize;
        const int boxY = y + (h - kCheckSize) / 2;
        tft.drawRect(boxX, boxY, kCheckSize, kCheckSize, fg);
        if (checked) {
            // Bird-like ✓: 4 px; green unfocused, black focused/edit (tft-ui skill).
            const uint16_t markFg =
                    (selected || editing) ? static_cast<uint16_t>(ST77XX_BLACK) : kCheckMarkGreen;
            const int half = kCheckStrokePx / 2;
            for (int i = 0; i < kCheckStrokePx; ++i) {
                const int dy = i - half;
                tft.drawLine(boxX - 1, boxY + 6 + dy, boxX + 5, boxY + 12 + dy, markFg);
                tft.drawLine(boxX + 5, boxY + 12 + dy, boxX + 15, boxY - 1 + dy, markFg);
            }
        }
    }

    void drawSettingsActionButton(int index, const char *text, bool confirm, bool selected) {
        const int y = settingsRowY(index);
        const int h = settingsRowH(index);
        tft.fillRect(0, y, TFT_WIDTH, h, ST77XX_BLACK);

        const int btnX = kActionBtnPadX;
        const int btnY = y + kActionBtnPadY;
        const int btnW = TFT_WIDTH - 2 * kActionBtnPadX;
        const int btnH = h - 2 * kActionBtnPadY;
        if (btnW <= 0 || btnH <= 0) {
            return;
        }
        const int radius = btnH / 2;
        const uint16_t fg = confirm ? kConfirmFg : kCancelFg;
        const uint16_t bg = confirm ? kConfirmBg : kCancelBg;
        tft.fillRoundRect(btnX, btnY, btnW, btnH, radius, bg);
        if (selected) {
            tft.drawRoundRect(btnX, btnY, btnW, btnH, radius, ST77XX_YELLOW);
            if (btnH > 4 && btnW > 4) {
                tft.drawRoundRect(btnX + 1, btnY + 1, btnW - 2, btnH - 2, radius > 0 ? radius - 1 : 0,
                                  ST77XX_YELLOW);
            }
        }

        tft.setFont(&FreeSansBold12pt7b);
        tft.setTextSize(1);
        tft.setTextColor(fg);
        int16_t x1 = 0;
        int16_t y1 = 0;
        uint16_t tw = 0;
        uint16_t th = 0;
        tft.getTextBounds(text, 0, 0, &x1, &y1, &tw, &th);
        int baseline = btnY + (btnH + static_cast<int>(th)) / 2;
        if (baseline > btnY + btnH - 2) {
            baseline = btnY + btnH - 2;
        }
        if (baseline < btnY + static_cast<int>(th)) {
            baseline = btnY + static_cast<int>(th);
        }
        const int x = btnX + (btnW - static_cast<int>(tw)) / 2 - x1;
        tft.setCursor(x, baseline);
        tft.print(text);
        tft.setFont(nullptr);
    }

    void formatSettingsRow(const UiState &state, SettingsFocus focus, char *name, size_t nameSize,
                           char *value, size_t valueSize, uint16_t *idleBgOut) {
        *idleBgOut = ST77XX_BLACK;
        name[0] = '\0';
        value[0] = '\0';
        switch (focus) {
            case SettingsFocus::Leak:
                snprintf(name, nameSize, "LEAK");
                snprintf(value, valueSize, "%us", state.draft.leakDetectSec);
                break;
            case SettingsFocus::Weak:
                snprintf(name, nameSize, "WEAK");
                snprintf(value, valueSize, "%us", state.draft.pumpWeakSec);
                break;
            case SettingsFocus::SensorMax:
                snprintf(name, nameSize, "SENS");
                snprintf(value, valueSize, "%.1f", toAtm(state.draft.sensorMaxMpa));
                break;
            case SettingsFocus::SensorMinVolts:
                snprintf(name, nameSize, "VMIN");
                snprintf(value, valueSize, "%.3f", state.draft.sensorMinVolts);
                break;
            case SettingsFocus::SensorMaxVolts:
                snprintf(name, nameSize, "VMAX");
                snprintf(value, valueSize, "%.3f", state.draft.sensorMaxVolts);
                break;
            case SettingsFocus::SamplesCount:
                snprintf(name, nameSize, "SAMP");
                snprintf(value, valueSize, "%d", state.draft.samplesCount);
                break;
            case SettingsFocus::MeasureIntervalMs:
                snprintf(name, nameSize, "INTV");
                snprintf(value, valueSize, "%d", state.draft.measureIntervalMs);
                break;
            case SettingsFocus::MeasurementsCount:
                snprintf(name, nameSize, "MCNT");
                snprintf(value, valueSize, "%d", state.draft.measurementsCount);
                break;
            case SettingsFocus::WifiEnabled:
                snprintf(name, nameSize, "WIFI");
                break;
            case SettingsFocus::OtaEnabled:
                snprintf(name, nameSize, "OTA");
                break;
            case SettingsFocus::Save:
                snprintf(name, nameSize, "SAVE");
                break;
            case SettingsFocus::Cancel:
                snprintf(name, nameSize, "CANCEL");
                break;
            case SettingsFocus::Count:
                break;
        }
    }

    void paintSettingsRow(const UiState &state, SettingsFocus focus) {
        if (focus >= SettingsFocus::Count) {
            return;
        }
        char name[16];
        char value[24];
        uint16_t idleBg = ST77XX_BLACK;
        formatSettingsRow(state, focus, name, sizeof(name), value, sizeof(value), &idleBg);
        const bool selected = state.focus == focus;
        if (focus == SettingsFocus::Save) {
            drawSettingsActionButton(static_cast<int>(focus), name, true, selected);
            return;
        }
        if (focus == SettingsFocus::Cancel) {
            drawSettingsActionButton(static_cast<int>(focus), name, false, selected);
            return;
        }
        const bool editing = selected && state.settingsEditing;
        if (focus == SettingsFocus::WifiEnabled) {
            drawSettingsCheckboxRow(static_cast<int>(focus), name, state.draftWifiEnabled, selected,
                                    editing);
            return;
        }
        if (focus == SettingsFocus::OtaEnabled) {
            drawSettingsCheckboxRow(static_cast<int>(focus), name, state.draftOtaEnabled, selected,
                                    editing);
            return;
        }
        drawSettingsRow(static_cast<int>(focus), name, value, selected, editing, idleBg);
    }

    void drawAllSettingsRows(const UiState &state) {
        for (uint8_t i = 0; i < static_cast<uint8_t>(SettingsFocus::Count); ++i) {
            paintSettingsRow(state, static_cast<SettingsFocus>(i));
        }
    }

    bool settingsRowValueChanged(const UiState &prev, const UiState &cur, SettingsFocus focus) {
        switch (focus) {
            case SettingsFocus::Leak:
                return prev.draft.leakDetectSec != cur.draft.leakDetectSec;
            case SettingsFocus::Weak:
                return prev.draft.pumpWeakSec != cur.draft.pumpWeakSec;
            case SettingsFocus::SensorMax:
                return !nearlyEq(prev.draft.sensorMaxMpa, cur.draft.sensorMaxMpa, 0.001f);
            case SettingsFocus::SensorMinVolts:
                return !nearlyEq(prev.draft.sensorMinVolts, cur.draft.sensorMinVolts, 0.001f);
            case SettingsFocus::SensorMaxVolts:
                return !nearlyEq(prev.draft.sensorMaxVolts, cur.draft.sensorMaxVolts, 0.001f);
            case SettingsFocus::SamplesCount:
                return prev.draft.samplesCount != cur.draft.samplesCount;
            case SettingsFocus::MeasureIntervalMs:
                return prev.draft.measureIntervalMs != cur.draft.measureIntervalMs;
            case SettingsFocus::MeasurementsCount:
                return prev.draft.measurementsCount != cur.draft.measurementsCount;
            case SettingsFocus::WifiEnabled:
                return prev.draftWifiEnabled != cur.draftWifiEnabled;
            case SettingsFocus::OtaEnabled:
                return prev.draftOtaEnabled != cur.draftOtaEnabled;
            case SettingsFocus::Save:
            case SettingsFocus::Cancel:
            case SettingsFocus::Count:
                return false;
        }
        return false;
    }

    void drawSettingsIncremental(const UiState &prev, const UiState &cur, bool first) {
        if (first) {
            drawAllSettingsRows(cur);
            return;
        }
        if (prev.focus != cur.focus || prev.settingsEditing != cur.settingsEditing) {
            paintSettingsRow(cur, prev.focus);
            paintSettingsRow(cur, cur.focus);
        }
        for (uint8_t i = 0; i < static_cast<uint8_t>(SettingsFocus::Count); ++i) {
            const auto focus = static_cast<SettingsFocus>(i);
            if (focus == prev.focus || focus == cur.focus) {
                continue;
            }
            if (settingsRowValueChanged(prev, cur, focus)) {
                paintSettingsRow(cur, focus);
            }
        }
        if (prev.focus == cur.focus && settingsRowValueChanged(prev, cur, cur.focus)) {
            paintSettingsRow(cur, cur.focus);
        }
    }

    bool draftChanged(const PressureSettings &a, const PressureSettings &b) {
        return a.leakDetectSec != b.leakDetectSec || a.pumpWeakSec != b.pumpWeakSec ||
               !nearlyEq(a.sensorMaxMpa, b.sensorMaxMpa, 0.001f) ||
               !nearlyEq(a.sensorMinVolts, b.sensorMinVolts, 0.001f) ||
               !nearlyEq(a.sensorMaxVolts, b.sensorMaxVolts, 0.001f) ||
               a.samplesCount != b.samplesCount ||
               a.measureIntervalMs != b.measureIntervalMs ||
               a.measurementsCount != b.measurementsCount;
    }
} // namespace

namespace Display {
    void begin(bool rotate180) {
        spiTft.begin(PIN_TFT_SCLK, PIN_TFT_MISO, PIN_TFT_MOSI, PIN_TFT_CS);
        tft.init(TFT_WIDTH, TFT_HEIGHT);
        uint8_t rotation = 2;
        if (rotate180) {
            rotation = static_cast<uint8_t>((rotation + 2) % 4);
        }
        tft.setRotation(rotation);
        tft.fillScreen(ST77XX_BLACK);
        gHasLast = false;
        gSettingsLayout = false;
    }

    void render(const UiState &state) {
        if (state.mode == UiMode::Settings) {
            if (!gSettingsLayout) {
                gSettingsLayout = true;
                gHasLast = false;
            }
            const bool need =
                    !gHasLast || gLast.focus != state.focus ||
                    gLast.settingsEditing != state.settingsEditing ||
                    draftChanged(gLast.draft, state.draft) ||
                    gLast.draftWifiEnabled != state.draftWifiEnabled ||
                    gLast.draftOtaEnabled != state.draftOtaEnabled || gLast.mode != state.mode;
            if (need) {
                drawSettingsIncremental(gLast, state, !gHasLast);
                gLast = state;
                gHasLast = true;
            }
            return;
        }

        // Leaving settings: clear only uncovered strips, then run-mode bar painters.
        // (Pump painter draws the icon only; MAX/MAC leave a 5px gap.)
        if (gSettingsLayout) {
            gSettingsLayout = false;
            gHasLast = false;
            tft.fillRect(0, kYMax + kHMax, TFT_WIDTH, kYMac - (kYMax + kHMax), ST77XX_BLACK);
            tft.fillRect(0, kYPump, TFT_WIDTH, kHPump, ST77XX_BLACK);
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
                gLast.otaActive != state.otaActive ||
                gLast.wifiRssiPercent != state.wifiRssiPercent;
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
