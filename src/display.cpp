#include "display.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <SPI.h>
#include <cmath>

#include "pins.h"
#include "settings.h"

namespace {
SPIClass spiTft(FSPI);
Adafruit_ST7789 tft(&spiTft, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

UiState gLast{};
bool gHasLast = false;
bool gFailLayout = false;
bool gSettingsLayout = false;

// Main screen contiguous bands (320px).
constexpr int kYMax = 0;
constexpr int kHMax = 52;      // was 64; shortened so current can move up 12
constexpr int kYCurrent = 52;  // was 64; up 12
constexpr int kHCurrent = 106;
constexpr int kYPump = 158;    // was 170; up 12
constexpr int kHPump = 100;
constexpr int kYMin = 258;
constexpr int kHMin = 62;      // was 70; -8

constexpr uint16_t kDarkGreen = 0x0400;
constexpr uint16_t kPumpOffGray = 0x8410;
constexpr int kPumpIconW = 112;
constexpr int kPumpIconH = 78;
// Top of pump band → icon y=159 (12px above previous centered y=171).
constexpr int kPumpIconPadY = 1;

// Settings screen: 5 equal rows.
constexpr int kSettingsRowH = 64;

static_assert(kYMax + kHMax == kYCurrent, "gap max/current");
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

// #region agent log
void dbgLog(const char* hypothesisId, const char* message, int fillScreen,
            int bottomCoveredTo) {
  Serial.printf(
      "{\"sessionId\":\"2ce4f1\",\"hypothesisId\":\"%s\",\"location\":\"display.cpp:render\","
      "\"message\":\"%s\",\"data\":{\"fillScreen\":%d,\"bottomTo\":%d,\"tftH\":%d,\"mode\":%u},"
      "\"timestamp\":%lu,\"runId\":\"settings-ui\"}\n",
      hypothesisId, message, fillScreen, bottomCoveredTo, TFT_HEIGHT,
      static_cast<unsigned>(0), static_cast<unsigned long>(millis()));
}
// #endregion

float toAtm(float mpa) {
  return mpa / PRESSURE_ATM_MPA;
}

void drawBar(int y, int h, const char* text, uint16_t fg, uint16_t bg,
             const GFXfont* font, bool centerH = false) {
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

void drawMax(const UiState& state) {
  char buf[32];
  snprintf(buf, sizeof(buf), "MAX %.2f", toAtm(state.maxMpa));
  const bool editing = state.mode == UiMode::EditMax;
  const uint16_t fg = editing ? ST77XX_BLACK : ST77XX_YELLOW;
  const uint16_t bg = editing ? ST77XX_YELLOW : ST77XX_RED;
  drawBar(kYMax, kHMax, buf, fg, bg, &FreeSansBold24pt7b);
}

void drawCurrent(const UiState& state) {
  char buf[24];
  snprintf(buf, sizeof(buf), "%.2f", toAtm(state.pressureMpa));
  drawBar(kYCurrent, kHCurrent, buf, ST77XX_CYAN, ST77XX_BLACK, &FreeSansBold24pt7b,
          true);
}

void drawPump(const UiState& state) {
  tft.fillRect(0, kYPump, TFT_WIDTH, kHPump, ST77XX_BLACK);
  drawPump(state.pumpOn ? ST77XX_GREEN : kPumpOffGray);
}

void drawMin(const UiState& state) {
  char buf[32];
  snprintf(buf, sizeof(buf), "MIN %.2f", toAtm(state.minMpa));
  const bool editing = state.mode == UiMode::EditMin;
  const uint16_t fg = editing ? ST77XX_BLACK : ST77XX_WHITE;
  const uint16_t bg = editing ? ST77XX_YELLOW : kDarkGreen;
  drawBar(kYMin, kHMin, buf, fg, bg, &FreeSansBold24pt7b);
}

void drawFail(const UiState& state) {
  char buf[24];
  snprintf(buf, sizeof(buf), "%.2f", toAtm(state.pressureMpa));
  drawBar(0, 100, "FAIL", ST77XX_RED, ST77XX_BLACK, &FreeSansBold24pt7b);
  drawBar(100, 110, buf, ST77XX_WHITE, ST77XX_BLACK, &FreeSansBold24pt7b);
  drawBar(210, 110, "Reboot", ST77XX_WHITE, ST77XX_BLACK, &FreeSansBold18pt7b);
}

void drawSettingsRow(int index, const char* text, bool selected) {
  const int y = index * kSettingsRowH;
  const uint16_t fg = selected ? ST77XX_BLACK : ST77XX_WHITE;
  const uint16_t bg = selected ? ST77XX_YELLOW : ST77XX_BLACK;
  drawBar(y, kSettingsRowH, text, fg, bg, &FreeSansBold12pt7b);
}

void drawSettings(const UiState& state) {
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

bool draftChanged(const PressureSettings& a, const PressureSettings& b) {
  return a.leakDetectSec != b.leakDetectSec || a.pumpWeakSec != b.pumpWeakSec ||
         !nearlyEq(a.sensorMaxMpa, b.sensorMaxMpa, 0.001f);
}
}  // namespace

namespace Display {

void begin() {
  spiTft.begin(PIN_TFT_SCLK, -1 /* MISO unused */, PIN_TFT_MOSI, PIN_TFT_CS);
  tft.init(TFT_WIDTH, TFT_HEIGHT);
  // #region agent log
  constexpr uint8_t kRotation = 2;
  tft.setRotation(kRotation);
  Serial.printf(
      "{\"sessionId\":\"2ce4f1\",\"hypothesisId\":\"S\",\"location\":\"display.cpp:begin\","
      "\"message\":\"tft_init\",\"data\":{\"rotation\":%u,\"w\":%d,\"h\":%d},"
      "\"timestamp\":%lu,\"runId\":\"settings-ui\"}\n",
      static_cast<unsigned>(kRotation), TFT_WIDTH, TFT_HEIGHT,
      static_cast<unsigned long>(millis()));
  // #endregion
  tft.fillScreen(ST77XX_BLACK);
  gHasLast = false;
  gFailLayout = false;
  gSettingsLayout = false;
}

void render(const UiState& state) {
  if (state.mode == UiMode::Fail) {
    if (!gFailLayout) {
      // #region agent log
      dbgLog("S", "enter_fail", 1, TFT_HEIGHT);
      // #endregion
      tft.fillScreen(ST77XX_BLACK);
      gFailLayout = true;
      gSettingsLayout = false;
      gHasLast = false;
    }
    const bool needFailRedraw =
        !gHasLast || !nearlyEq(gLast.pressureMpa, state.pressureMpa, 0.001f) ||
        gLast.mode != state.mode;
    if (needFailRedraw) {
      drawFail(state);
      gLast = state;
      gHasLast = true;
    }
    return;
  }

  if (state.mode == UiMode::Settings) {
    if (!gSettingsLayout) {
      // #region agent log
      dbgLog("S", "enter_settings", 1, TFT_HEIGHT);
      // #endregion
      tft.fillScreen(ST77XX_BLACK);
      gSettingsLayout = true;
      gFailLayout = false;
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

  // Leaving special layouts
  if (gFailLayout || gSettingsLayout) {
    // #region agent log
    dbgLog("S", "leave_special_layout", 1, TFT_HEIGHT);
    // #endregion
    tft.fillScreen(ST77XX_BLACK);
    gFailLayout = false;
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
  const bool redrawCur =
      first || !nearlyEq(gLast.pressureMpa, state.pressureMpa, 0.001f);
  const bool redrawPump = first || gLast.pumpOn != state.pumpOn;
  const bool redrawMin =
      first || !nearlyEq(gLast.minMpa, state.minMpa, 0.0005f) || hlMin != wasHlMin;

  if (!redrawMax && !redrawCur && !redrawPump && !redrawMin) {
    return;
  }

  // #region agent log
  dbgLog("S", "partial_redraw", 0, kYMin + kHMin);
  // #endregion

  if (redrawMax) {
    drawMax(state);
  }
  if (redrawCur) {
    drawCurrent(state);
  }
  if (redrawPump) {
    drawPump(state);
  }
  if (redrawMin) {
    drawMin(state);
  }

  gLast = state;
  gHasLast = true;
}

}  // namespace Display
