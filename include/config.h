#pragma once

#include <Adafruit_TinyUSB.h>

// ============================================================
// Pin Configuration — nice!nano v2 (nRF52840)
//
// nice!nano physical layout (front, USB up):
//
//         ┌──────────┐
//         │  USB-C   │
//    ─────┤          ├─────
//   P0.06 ┤ D0   RAW ├ battery
//   P0.08 ┤ D1   GND ├
//     GND ┤      RST ├
//     GND ┤      VCC ├ 3.3V out
//   P0.17 ┤ D2   D21 ├ P0.31  (A2)
//   P0.20 ┤ D3   D20 ├ P0.29  (A1)
//   P0.22 ┤ D4   D19 ├ P0.02  (A0) ← slider here
//   P0.24 ┤ D5   D18 ├ P1.15  (no ADC!)
//   P1.00 ┤ D6   D15 ├ P1.13
//   P0.11 ┤ D7   D14 ├ P1.11
//   P1.04 ┤ D8   D16 ├ P0.10
//   P1.06 ┤ D9   D10 ├ P0.09
//         └──────────┘
// ============================================================

// Slider (potentiometer) — wire to D19 position (P0.02, AIN0)
#define PIN_SLIDER      A0

// Button pins (active LOW with internal pull-up)
// Left side D2–D7 on the nice!nano
#define PIN_BTN_1       2   // D2  — P0.17
#define PIN_BTN_2       3   // D3  — P0.20
#define PIN_BTN_3       4   // D4  — P0.22
#define PIN_BTN_4       5   // D5  — P0.24
#define PIN_BTN_5       6   // D6  — P1.00
#define PIN_BTN_6       7   // D7  — P0.11

#define NUM_BUTTONS     6

static const uint8_t BUTTON_PINS[NUM_BUTTONS] = {
    PIN_BTN_1, PIN_BTN_2, PIN_BTN_3,
    PIN_BTN_4, PIN_BTN_5, PIN_BTN_6
};

// ============================================================
// Key Mapping (HID keycodes from TinyUSB)
// ============================================================
// Button 1: Left Shift
// Button 2: A
// Button 3: S
// Button 4: D
// Button 5: F
// Button 6: Space

static const uint8_t BUTTON_KEYS[NUM_BUTTONS] = {
    HID_KEY_SHIFT_LEFT,  // Button 1
    HID_KEY_A,           // Button 2
    HID_KEY_S,           // Button 3
    HID_KEY_D,           // Button 4
    HID_KEY_F,           // Button 5
    HID_KEY_SPACE        // Button 6
};

// ============================================================
// HID Report IDs
// ============================================================
#define RID_KEYBOARD    1
#define RID_MOUSE       2

// ============================================================
// Tuning Parameters
// ============================================================

// Button debounce time (ms)
#define DEBOUNCE_MS         20

// Invert mouse X direction (set to -1 to reverse, 1 for normal)
#define MOUSE_INVERT_X      -1

// Fixed absolute Y position (0-32767). Center = 16384.
#define SLIDER_ABS_Y        16384

// EMA smoothing factor (0.0 - 1.0). Lower = smoother but laggier.
#define SLIDER_SMOOTHING    0.3f

// How often to read inputs (ms)
#define POLL_INTERVAL_MS    2
