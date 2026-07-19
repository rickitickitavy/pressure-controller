#include "encoder.h"

namespace {
int gPinA = -1;
int gPinB = -1;
int gPinBtn = -1;

volatile int gSteps = 0;
volatile uint8_t gLastAb = 0;

int gBtnStable = HIGH;
int gBtnReading = HIGH;
unsigned long gBtnLastChangeMs = 0;
unsigned long gBtnDownMs = 0;
bool gPressPending = false;
bool gHoldPending = false;
bool gLongPending = false;
bool gHold1FiredThisHold = false;
bool gLong3FiredThisHold = false;

constexpr unsigned long kDebounceMs = 40;
constexpr unsigned long kHoldPressMs = 1000;
constexpr unsigned long kLongPressMs = 3000;

constexpr int8_t kQuadTable[16] = {
    0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0,
};

void IRAM_ATTR onEncoderIsr() {
  const uint8_t a = digitalRead(gPinA) ? 1 : 0;
  const uint8_t b = digitalRead(gPinB) ? 1 : 0;
  const uint8_t ab = (a << 1) | b;
  const int8_t delta = kQuadTable[(gLastAb << 2) | ab];
  gLastAb = ab;
  if (delta != 0) {
    gSteps += delta;
  }
}
}  // namespace

namespace Encoder {

void begin(int pinA, int pinB, int pinBtn) {
  gPinA = pinA;
  gPinB = pinB;
  gPinBtn = pinBtn;

  pinMode(gPinA, INPUT_PULLUP);
  pinMode(gPinB, INPUT_PULLUP);
  pinMode(gPinBtn, INPUT_PULLUP);

  const uint8_t a = digitalRead(gPinA) ? 1 : 0;
  const uint8_t b = digitalRead(gPinB) ? 1 : 0;
  gLastAb = (a << 1) | b;

  attachInterrupt(digitalPinToInterrupt(gPinA), onEncoderIsr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(gPinB), onEncoderIsr, CHANGE);

  gBtnStable = digitalRead(gPinBtn);
  gBtnReading = gBtnStable;
  gBtnLastChangeMs = millis();
}

void update() {
  const int reading = digitalRead(gPinBtn);
  if (reading != gBtnReading) {
    gBtnReading = reading;
    gBtnLastChangeMs = millis();
  }
  if ((millis() - gBtnLastChangeMs) >= kDebounceMs && reading != gBtnStable) {
    gBtnStable = reading;
    if (gBtnStable == LOW) {
      gBtnDownMs = millis();
      gHold1FiredThisHold = false;
      gLong3FiredThisHold = false;
    } else {
      // Released: short click only if never reached 1s hold.
      if (!gHold1FiredThisHold) {
        gPressPending = true;
      }
    }
  }

  if (gBtnStable == LOW) {
    const unsigned long held = millis() - gBtnDownMs;
    if (!gHold1FiredThisHold && held >= kHoldPressMs) {
      gHoldPending = true;
      gHold1FiredThisHold = true;
    }
    if (!gLong3FiredThisHold && held >= kLongPressMs) {
      gLongPending = true;
      gLong3FiredThisHold = true;
    }
  }
}

int consumeSteps() {
  noInterrupts();
  const int raw = gSteps;
  const int detents = raw / 4;
  gSteps = raw % 4;
  interrupts();
  return detents;
}

bool consumePress() {
  if (!gPressPending) {
    return false;
  }
  gPressPending = false;
  return true;
}

bool consumeHoldPress() {
  if (!gHoldPending) {
    return false;
  }
  gHoldPending = false;
  return true;
}

bool consumeLongPress() {
  if (!gLongPending) {
    return false;
  }
  gLongPending = false;
  return true;
}

}  // namespace Encoder
