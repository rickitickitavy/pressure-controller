#include "settings.h"

#include <Preferences.h>

namespace {
Preferences prefs;
constexpr const char* kNs = "pump";
constexpr const char* kMin = "min";
constexpr const char* kMax = "max";
constexpr const char* kLeak = "leak";
constexpr const char* kWeak = "weak";
constexpr const char* kSens = "sens";
}  // namespace

namespace Settings {

void begin() {
  prefs.begin(kNs, false);
}

void clampAdvanced(PressureSettings& s) {
  if (s.leakDetectSec < LEAK_SEC_MIN) {
    s.leakDetectSec = LEAK_SEC_MIN;
  }
  if (s.leakDetectSec > LEAK_SEC_MAX) {
    s.leakDetectSec = LEAK_SEC_MAX;
  }
  if (s.pumpWeakSec < WEAK_SEC_MIN) {
    s.pumpWeakSec = WEAK_SEC_MIN;
  }
  if (s.pumpWeakSec > WEAK_SEC_MAX) {
    s.pumpWeakSec = WEAK_SEC_MAX;
  }
  if (s.sensorMaxMpa < SENSOR_MAX_MIN_MPA) {
    s.sensorMaxMpa = SENSOR_MAX_MIN_MPA;
  }
  if (s.sensorMaxMpa > SENSOR_MAX_MAX_MPA) {
    s.sensorMaxMpa = SENSOR_MAX_MAX_MPA;
  }
  // Snap to 0.1 atm steps
  const float steps = roundf(s.sensorMaxMpa / SENSOR_MAX_STEP_MPA);
  s.sensorMaxMpa = steps * SENSOR_MAX_STEP_MPA;
  if (s.sensorMaxMpa < SENSOR_MAX_MIN_MPA) {
    s.sensorMaxMpa = SENSOR_MAX_MIN_MPA;
  }
  if (s.sensorMaxMpa > SENSOR_MAX_MAX_MPA) {
    s.sensorMaxMpa = SENSOR_MAX_MAX_MPA;
  }
}

void clampPair(float& minMpa, float& maxMpa, float sensorMaxMpa) {
  if (sensorMaxMpa < SENSOR_MAX_MIN_MPA) {
    sensorMaxMpa = SENSOR_MAX_MIN_MPA;
  }
  if (minMpa < 0.0f) {
    minMpa = 0.0f;
  }
  if (maxMpa > sensorMaxMpa) {
    maxMpa = sensorMaxMpa;
  }
  if (minMpa > sensorMaxMpa - PRESSURE_MIN_GAP_MPA) {
    minMpa = sensorMaxMpa - PRESSURE_MIN_GAP_MPA;
  }
  if (maxMpa < PRESSURE_MIN_GAP_MPA) {
    maxMpa = PRESSURE_MIN_GAP_MPA;
  }
  if (minMpa >= maxMpa) {
    if (maxMpa - PRESSURE_MIN_GAP_MPA >= 0.0f) {
      minMpa = maxMpa - PRESSURE_MIN_GAP_MPA;
    } else {
      maxMpa = minMpa + PRESSURE_MIN_GAP_MPA;
      if (maxMpa > sensorMaxMpa) {
        maxMpa = sensorMaxMpa;
        minMpa = maxMpa - PRESSURE_MIN_GAP_MPA;
      }
    }
  }
}

PressureSettings load() {
  PressureSettings s;
  s.minMpa = prefs.getFloat(kMin, s.minMpa);
  s.maxMpa = prefs.getFloat(kMax, s.maxMpa);
  s.leakDetectSec = prefs.getUShort(kLeak, s.leakDetectSec);
  s.pumpWeakSec = prefs.getUShort(kWeak, s.pumpWeakSec);
  s.sensorMaxMpa = prefs.getFloat(kSens, s.sensorMaxMpa);
  clampAdvanced(s);
  clampPair(s.minMpa, s.maxMpa, s.sensorMaxMpa);
  return s;
}

void save(const PressureSettings& s) {
  PressureSettings out = s;
  clampAdvanced(out);
  clampPair(out.minMpa, out.maxMpa, out.sensorMaxMpa);
  prefs.putFloat(kMin, out.minMpa);
  prefs.putFloat(kMax, out.maxMpa);
  prefs.putUShort(kLeak, out.leakDetectSec);
  prefs.putUShort(kWeak, out.pumpWeakSec);
  prefs.putFloat(kSens, out.sensorMaxMpa);
}

}  // namespace Settings
