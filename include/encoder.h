#pragma once

#include <Arduino.h>

namespace Encoder {

void begin(int pinA, int pinB, int pinBtn);
void update();

// Cumulative steps since last consume (positive = CW).
int consumeSteps();

// Short click: press+release before 1 second.
bool consumePress();

// Held for >= 1 second (fires once per hold).
bool consumeHoldPress();

// Held for >= 3 seconds (fires once per hold).
bool consumeLongPress();

}  // namespace Encoder
