#pragma once

#include <bluefruit.h>

// ============================================================
// Pin Configuration - nRF52840 Pro Micro
// ============================================================

// Slider (potentiometer) - must be ADC-capable (AIN) pin
// A0 = P0.02 (AIN0) on SparkFun Pro Micro nRF52840
#define PIN_SLIDER      A0

// Button pins (active LOW with internal pull-up)
// Using Pro Micro digital pin positions
#define PIN_BTN_1       2
#define PIN_BTN_2       3
#define PIN_BTN_3       4
#define PIN_BTN_4       5
#define PIN_BTN_5       6
#define PIN_BTN_6       7

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
// Tuning Parameters
// ============================================================

// Button debounce time (ms)
#define DEBOUNCE_MS         20

// Deadzone: ignore small slider movements (ADC units)
#define SLIDER_DEADZONE     50

// Mouse movement scaling
#define MOUSE_SPEED         8

// Invert mouse X direction (set to -1 to reverse, 1 for normal)
#define MOUSE_INVERT_X      -1

// EMA smoothing factor (0.0 - 1.0). Lower = smoother but laggier.
#define SLIDER_SMOOTHING    0.3f

// How often to read inputs (ms)
#define POLL_INTERVAL_MS    2
