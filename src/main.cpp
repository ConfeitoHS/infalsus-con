#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include "config.h"

// USB HID report descriptor: keyboard + mouse combo
uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(RID_KEYBOARD)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(RID_MOUSE))
};

// USB HID device
Adafruit_USBD_HID usb_hid(desc_hid_report, sizeof(desc_hid_report),
                           HID_ITF_PROTOCOL_NONE, 2, false);

// Button state tracking
static bool     btn_pressed[NUM_BUTTONS]  = {};
static uint32_t btn_last_change[NUM_BUTTONS] = {};

// Keyboard report state
static uint8_t active_modifier  = 0;
static uint8_t active_keys[6]   = {};
static uint8_t active_key_count = 0;

// Slider state tracking
static int   slider_prev   = -1;
static float slider_smooth = -1;

// --------------------------------------------------------
// Send the current keyboard state as a 6KRO HID report
// --------------------------------------------------------
static void send_keyboard_report() {
    uint8_t keycodes[6] = {};
    for (uint8_t i = 0; i < 6 && i < active_key_count; i++) {
        keycodes[i] = active_keys[i];
    }
    usb_hid.keyboardReport(RID_KEYBOARD, active_modifier, keycodes);
}

// --------------------------------------------------------
// Read a single button with debounce.
// Buttons are wired active-LOW (pressed = LOW).
// --------------------------------------------------------
static void handle_button(uint8_t idx) {
    if (!USBDevice.mounted()) return;

    bool raw = (digitalRead(BUTTON_PINS[idx]) == LOW);
    uint32_t now = millis();

    if (raw != btn_pressed[idx] &&
        (now - btn_last_change[idx]) >= DEBOUNCE_MS) {
        btn_pressed[idx] = raw;
        btn_last_change[idx] = now;

        uint8_t keycode = BUTTON_KEYS[idx];
        bool is_modifier = (keycode >= HID_KEY_CONTROL_LEFT &&
                            keycode <= HID_KEY_GUI_RIGHT);

        if (raw) {  // pressed
            if (is_modifier) {
                active_modifier |= (1 << (keycode - HID_KEY_CONTROL_LEFT));
            } else if (active_key_count < 6) {
                active_keys[active_key_count++] = keycode;
            }
        } else {    // released
            if (is_modifier) {
                active_modifier &= ~(1 << (keycode - HID_KEY_CONTROL_LEFT));
            } else {
                for (uint8_t i = 0; i < active_key_count; i++) {
                    if (active_keys[i] == keycode) {
                        for (uint8_t j = i; j < active_key_count - 1; j++) {
                            active_keys[j] = active_keys[j + 1];
                        }
                        active_key_count--;
                        active_keys[active_key_count] = 0;
                        break;
                    }
                }
            }
        }

        send_keyboard_report();
    }
}

// --------------------------------------------------------
// Read the slider potentiometer and move mouse X axis.
// --------------------------------------------------------
static void handle_slider() {
    if (!USBDevice.mounted()) return;

    int raw = analogRead(PIN_SLIDER);

    // First reading — just store, don't move
    if (slider_prev < 0) {
        slider_prev = raw;
        slider_smooth = raw;
        return;
    }

    // EMA low-pass filter to suppress ADC noise / jitter
    slider_smooth = slider_smooth + SLIDER_SMOOTHING * (raw - slider_smooth);

    int filtered = (int)(slider_smooth + 0.5f);
    int delta = filtered - slider_prev;

    // Apply deadzone
    if (abs(delta) < SLIDER_DEADZONE) {
        return;
    }

    slider_prev = filtered;

    // Scale delta to mouse movement range (-127..127), apply inversion
    int move_x = (delta * MOUSE_SPEED * MOUSE_INVERT_X) / 100;
    move_x = constrain(move_x, -127, 127);

    if (move_x != 0) {
        usb_hid.mouseReport(RID_MOUSE, 0, move_x, 0, 0, 0);
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

    // Configure slider ADC (12-bit)
    analogReadResolution(12);
    pinMode(PIN_SLIDER, INPUT);

    // Initialize USB HID
    usb_hid.begin();

    // Wait for USB to be ready
    while (!USBDevice.mounted()) {
        delay(1);
    }
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
