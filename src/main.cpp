#include <Arduino.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <USBHIDMouse.h>
#include "config.h"

// USB HID devices
USBHIDKeyboard Keyboard;
USBHIDMouse Mouse;

// Button state tracking
static bool     btn_pressed[NUM_BUTTONS]  = {};
static uint32_t btn_last_change[NUM_BUTTONS] = {};

// Slider state tracking
static int slider_prev = -1;

// --------------------------------------------------------
// Read a single button with debounce.
// Buttons are wired active-LOW (pressed = LOW).
// --------------------------------------------------------
static void handle_button(uint8_t idx) {
    bool raw = (digitalRead(BUTTON_PINS[idx]) == LOW);
    uint32_t now = millis();

    if (raw != btn_pressed[idx] &&
        (now - btn_last_change[idx]) >= DEBOUNCE_MS) {
        btn_pressed[idx] = raw;
        btn_last_change[idx] = now;

        if (raw) {
            Keyboard.press(BUTTON_KEYS[idx]);
        } else {
            Keyboard.release(BUTTON_KEYS[idx]);
        }
    }
}

// --------------------------------------------------------
// Read the slider potentiometer and move mouse X axis.
// Converts absolute slider position to relative mouse
// movement so the cursor tracks the slider position.
// --------------------------------------------------------
static void handle_slider() {
    int raw = analogRead(PIN_SLIDER);

    // First reading — just store, don't move
    if (slider_prev < 0) {
        slider_prev = raw;
        return;
    }

    int delta = raw - slider_prev;

    // Apply deadzone
    if (abs(delta) < SLIDER_DEADZONE) {
        return;
    }

    slider_prev = raw;

    // Scale delta to mouse movement range (-127..127)
    int move = (delta * MOUSE_SPEED) / 100;
    move = constrain(move, -127, 127);

    if (move != 0) {
        Mouse.move(move, 0, 0);
    }
}

// --------------------------------------------------------
// Setup
// --------------------------------------------------------
void setup() {
    // Configure button pins with internal pull-up
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    }

    // Configure slider ADC
    analogReadResolution(12);
    pinMode(PIN_SLIDER, INPUT);

    // Start USB HID
    Keyboard.begin();
    Mouse.begin();
    USB.begin();
}

// --------------------------------------------------------
// Main loop
// --------------------------------------------------------
void loop() {
    // Process all buttons
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        handle_button(i);
    }

    // Process slider
    handle_slider();

    delay(POLL_INTERVAL_MS);
}
