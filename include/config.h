#pragma once

#include <BleComboKeyboard.h>

// ============================================================
// Pin Configuration - ESP32-C3
// ============================================================

// Slider (potentiometer) - must be ADC-capable pin
#define PIN_SLIDER      GPIO_NUM_0  // ADC1_CH0

// Button pins (active LOW with internal pull-up)
#define PIN_BTN_1       GPIO_NUM_1
#define PIN_BTN_2       GPIO_NUM_2
#define PIN_BTN_3       GPIO_NUM_3
#define PIN_BTN_4       GPIO_NUM_4
#define PIN_BTN_5       GPIO_NUM_5
#define PIN_BTN_6       GPIO_NUM_6

#define NUM_BUTTONS     6

static const uint8_t BUTTON_PINS[NUM_BUTTONS] = {
    PIN_BTN_1, PIN_BTN_2, PIN_BTN_3,
    PIN_BTN_4, PIN_BTN_5, PIN_BTN_6
};

// ============================================================
// Key Mapping
// ============================================================
// Button 1: Left Shift
// Button 2: A
// Button 3: S
// Button 4: D
// Button 5: F
// Button 6: Space

static const uint8_t BUTTON_KEYS[NUM_BUTTONS] = {
    KEY_LEFT_SHIFT,  // Button 1
    'a',             // Button 2
    's',             // Button 3
    'd',             // Button 4
    'f',             // Button 5
    ' '              // Button 6 (Space)
};

// ============================================================
// Tuning Parameters
// ============================================================

// Button debounce time (ms)
#define DEBOUNCE_MS         20

// Slider ADC range (12-bit: 0-4095)
#define SLIDER_ADC_MIN      0
#define SLIDER_ADC_MAX      4095

// Deadzone: ignore small slider movements (ADC units)
#define SLIDER_DEADZONE     50

// Mouse movement scaling
// This controls how aggressively the cursor follows the slider.
#define MOUSE_SPEED         8

// Invert mouse X direction (set to -1 to reverse, 1 for normal)
#define MOUSE_INVERT_X      -1

// EMA smoothing factor (0.0 – 1.0). Lower = smoother but laggier.
#define SLIDER_SMOOTHING    0.3f

// How often to read inputs (ms)
#define POLL_INTERVAL_MS    2
