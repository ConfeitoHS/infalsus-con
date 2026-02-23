#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include "config.h"

// Absolute mouse HID report descriptor (fallback if TinyUSB lacks it)
#ifndef TUD_HID_REPORT_DESC_ABSMOUSE
#define TUD_HID_REPORT_DESC_ABSMOUSE(...) \
  HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP      )              ,\
  HID_USAGE      ( HID_USAGE_DESKTOP_MOUSE     )              ,\
  HID_COLLECTION ( HID_COLLECTION_APPLICATION   )              ,\
    __VA_ARGS__ \
    HID_USAGE      ( HID_USAGE_DESKTOP_POINTER  )             ,\
    HID_COLLECTION ( HID_COLLECTION_PHYSICAL     )             ,\
      HID_USAGE_PAGE  ( HID_USAGE_PAGE_BUTTON  )              ,\
      HID_USAGE_MIN   ( 1                      )              ,\
      HID_USAGE_MAX   ( 5                      )              ,\
      HID_LOGICAL_MIN ( 0                      )              ,\
      HID_LOGICAL_MAX ( 1                      )              ,\
      HID_REPORT_COUNT( 5                      )              ,\
      HID_REPORT_SIZE ( 1                      )              ,\
      HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
      HID_REPORT_COUNT( 1                      )              ,\
      HID_REPORT_SIZE ( 3                      )              ,\
      HID_INPUT       ( HID_CONSTANT           )              ,\
      HID_USAGE_PAGE  ( HID_USAGE_PAGE_DESKTOP )              ,\
      HID_USAGE       ( HID_USAGE_DESKTOP_X    )              ,\
      HID_USAGE       ( HID_USAGE_DESKTOP_Y    )              ,\
      HID_LOGICAL_MIN_N ( 0, 2                 )              ,\
      HID_LOGICAL_MAX_N ( 0x7FFF, 2            )              ,\
      HID_REPORT_COUNT( 2                      )              ,\
      HID_REPORT_SIZE ( 16                     )              ,\
      HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
      HID_USAGE       ( HID_USAGE_DESKTOP_WHEEL )             ,\
      HID_LOGICAL_MIN ( 0x81                   )              ,\
      HID_LOGICAL_MAX ( 0x7f                   )              ,\
      HID_REPORT_COUNT( 1                      )              ,\
      HID_REPORT_SIZE ( 8                      )              ,\
      HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_RELATIVE ) ,\
    HID_COLLECTION_END                                         ,\
  HID_COLLECTION_END
#endif

// Absolute mouse report (must match the descriptor above)
typedef struct __attribute__((packed)) {
    uint8_t  buttons;
    uint16_t x;
    uint16_t y;
    int8_t   wheel;
} abs_mouse_report_t;

// USB HID report descriptor: keyboard + absolute mouse combo
uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(RID_KEYBOARD)),
    TUD_HID_REPORT_DESC_ABSMOUSE(HID_REPORT_ID(RID_MOUSE))
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
static float    slider_smooth = -1;
static uint16_t slider_last_x = 0xFFFF;

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
// Read the slider potentiometer and send absolute mouse X.
// ADC 12-bit (0-4095) → HID absolute (0-32767).
// --------------------------------------------------------
static void handle_slider() {
    if (!USBDevice.mounted()) return;

    int raw = analogRead(PIN_SLIDER);

    // First reading — just store
    if (slider_smooth < 0) {
        slider_smooth = raw;
        return;
    }

    // EMA low-pass filter to suppress ADC noise / jitter
    slider_smooth = slider_smooth + SLIDER_SMOOTHING * (raw - slider_smooth);

    // Map to HID absolute range (0-32767)
    uint16_t abs_x = (uint16_t)(slider_smooth * (32767.0f / 4095.0f));
    if (MOUSE_INVERT_X < 0) abs_x = 32767 - abs_x;

    // Only send when position actually changed
    if (abs_x == slider_last_x) return;
    slider_last_x = abs_x;

    abs_mouse_report_t report = {};
    report.x = abs_x;
    report.y = SLIDER_ABS_Y;
    usb_hid.sendReport(RID_MOUSE, &report, sizeof(report));
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
