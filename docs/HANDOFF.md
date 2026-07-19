# Pump controller — handoff / context

PlatformIO project for **ESP32-C3-DevKitC-02** (`espressif32` / Arduino).  
Path: `/home/dsporynkhin/Projects/cpp/home/presure-controller`

Use this file plus the plan copies in `docs/` when starting a new chat (`@docs/HANDOFF.md`).

## Hardware (current)

| Function | GPIO |
|----------|------|
| Pressure sensor (ADC) | 0 |
| Encoder A | 10 |
| Encoder B | 1 |
| Encoder button | 8 |
| Pump | 9 |
| ST7789 MOSI | 6 |
| ST7789 SCLK | 4 |
| ST7789 CS | 7 |
| ST7789 DC | 2 |
| ST7789 RST | 3 |

- Display: **240×320**, rotation **180°** (`setRotation(2)`), no backlight pin.
- Pins defined in [`include/pins.h`](../include/pins.h).

## Pressure scale

- ADC **0** → **-1** (vacuum / 0 psi).
- ADC **4095** → `sensorMaxMpa` (configurable in SETTINGS, default 0.5 MPa = 5 Atm).
- Display: atmospheres via `mpa / 0.1` (1 Atm = 0.1 MPa). No unit text on LCD.

## Main UI (top → bottom)

Band heights in [`src/display.cpp`](../src/display.cpp) (`kHMax` / `kHCurrent` / `kHPump` / `kHMin`):

1. **MAX** — yellow on red (edit: black on yellow); FreeSansBold 24pt  
2. **Current** — cyan on black, **horizontally centered**; FreeSansBold 24pt  
3. **Pump** — icon (not ON/OFF text); **green** when ON, **gray** when OFF  
4. **MIN** — white on dark green (edit: black on yellow); FreeSansBold 24pt  

Partial bar redraws only (no full-screen clear in normal updates).

## Modes

| Mode | How |
|------|-----|
| Run | Default control |
| Edit MAX / MIN | Short button: Run → EditMax → EditMin → Run; rotate ±0.001 MPa; idle **5 s** (button or rotate resets timer) |
| SETTINGS | Hold button **≥ 3 s** |
| Fail | **LEAK** timeout only; pump off; locked until reboot |

### Fail-safe / pump limits

When the pump turns **ON**:

1. **LEAK** (reach-min): if pressure has not reached **min** within `leakDetectSec` → enter **Fail** (latched until reboot).
2. **WEAK** (max continuous run): if pump stays ON longer than `pumpWeakSec` → **pump OFF only** (no Fail). May turn ON again on the next loop if pressure is still below min (fresh weak timer).

### SETTINGS screen

Rows: **LEAK**, **WEAK**, **SENS**, **SAVE**, **CANCEL**

- Short press: cycle focus  
- Rotate: change LEAK / WEAK / SENS  
- Hold **1 s** on SAVE / CANCEL: activate  
- Idle **10 s**: CANCEL  

| Setting | Range | Step | Default |
|---------|-------|------|---------|
| LEAK (reach min after pump ON) | 5–40 s | 1 s | 10 |
| WEAK (max pump ON; then OFF, no Fail) | 40–600 s | 1 s | 180 |
| SENS (sensor full-scale) | 2.0–50.0 Atm | 0.1 Atm | 5.0 |

Persisted with ESP32 `Preferences` (NVS). See [`include/settings.h`](../include/settings.h).

## Key source files

- `src/main.cpp` — FSM, control, settings apply  
- `src/display.cpp` — ST7789 UI + pump icon  
- `src/encoder.cpp` — quadrature + short / 1 s hold / 3 s hold  
- `src/pressure.cpp` — ADC → MPa  
- `src/settings.cpp` — NVS load/save  

## Build / flash

```bash
cd /home/dsporynkhin/Projects/cpp/home/presure-controller
pio run
pio run -t upload
pio device monitor   # 115200
```

## Debug note

Serial still has temporary NDJSON debug logs (session `2ce4f1`) in `display.cpp`, `main.cpp`, `pressure.cpp`. Safe to remove after verification.

## Plan copies in this folder

- `plan-pump-pressure-controller.md` — feature plan (kept in sync with current behavior)
- `plan-reset-edit-idle-timer.md` — idle timer on encoder rotation (**done**)
