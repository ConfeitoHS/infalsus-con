#include <Arduino.h>
#include <bluefruit.h>
#include "config.h"

// BLE services
BLEDis        bledis;   // Device Information Service
BLEBas        blebas;   // Battery Service
BLEHidAdafruit blehid;  // HID Service (keyboard + mouse combined)

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
    blehid.keyboardReport(active_modifier, keycodes);
}

// --------------------------------------------------------
// Read a single button with debounce.
// Buttons are wired active-LOW (pressed = LOW).
// --------------------------------------------------------
static void handle_button(uint8_t idx) {
    if (!Bluefruit.connected()) return;

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
    if (!Bluefruit.connected()) return;

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
        blehid.mouseMove((int8_t)move_x, 0);
    }
}

// --------------------------------------------------------
// BLE callbacks
// --------------------------------------------------------
static void connect_cb(uint16_t conn_handle) {
    (void)conn_handle;
    Serial.println("BLE connected");
}

static void disconnect_cb(uint16_t conn_handle, uint8_t reason) {
    (void)conn_handle;
    (void)reason;
    Serial.println("BLE disconnected");
}

// --------------------------------------------------------
// Setup
// --------------------------------------------------------
void setup() {
    Serial.begin(115200);

    // Configure button pins with internal pull-up
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    }

    // Configure slider ADC (12-bit for compatibility)
    analogReadResolution(12);
    pinMode(PIN_SLIDER, INPUT);

    // Initialize Bluefruit
    Bluefruit.begin();
    Bluefruit.setTxPower(4);
    Bluefruit.setName("infalsus-con");
    Bluefruit.Periph.setConnectCallback(connect_cb);
    Bluefruit.Periph.setDisconnectCallback(disconnect_cb);

    // Device Information Service
    bledis.setManufacturer("Anthropic");
    bledis.setModel("infalsus-con nRF52840");
    bledis.begin();

    // Battery Service
    blebas.begin();
    blebas.write(100);

    // HID Service (keyboard + mouse)
    blehid.begin();

    // Start advertising
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();
    Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_KEYBOARD);
    Bluefruit.Advertising.addService(blehid);
    Bluefruit.Advertising.addName();
    Bluefruit.Advertising.restartOnDisconnect(true);
    Bluefruit.Advertising.setInterval(32, 244);
    Bluefruit.Advertising.setFastTimeout(30);
    Bluefruit.Advertising.start(0);
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
