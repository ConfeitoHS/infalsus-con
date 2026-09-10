#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include "config.h"

// Absolute mouse HID report descriptor.
// Always use our own to guarantee the report struct matches exactly.
// Layout: buttons(1 byte) + x(2) + y(2) = 5 bytes total.
#define ABSMOUSE_REPORT_DESC(...) \
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
    HID_COLLECTION_END                                         ,\
  HID_COLLECTION_END

// Must match the descriptor above exactly: buttons(1) + x(2) + y(2) = 5 bytes
typedef struct __attribute__((packed)) {
    uint8_t  buttons;
    uint16_t x;
    uint16_t y;
} abs_mouse_report_t;

// USB HID report descriptor: keyboard + absolute mouse combo
uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(RID_KEYBOARD)),
    ABSMOUSE_REPORT_DESC(HID_REPORT_ID(RID_MOUSE))
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

// Keyboard report pending flag — set when a report needs to be
// (re)sent because the USB endpoint was busy on the previous attempt.
static bool kb_report_pending = false;

// --------------------------------------------------------
// Send the current keyboard state as a 6KRO HID report.
// Returns true if the USB send succeeded.
// --------------------------------------------------------
static bool send_keyboard_report() {
    uint8_t keycodes[6] = {};
    for (uint8_t i = 0; i < 6 && i < active_key_count; i++) {
        keycodes[i] = active_keys[i];
    }
    return usb_hid.keyboardReport(RID_KEYBOARD, active_modifier, keycodes);
}

// --------------------------------------------------------
// Scan all buttons with debounce, then send ONE report
// if anything changed.  If the USB endpoint was busy the
// report is retried on the next poll cycle.
// --------------------------------------------------------
static void handle_buttons() {
    if (!USBDevice.mounted()) return;

    uint32_t now = millis();

    for (uint8_t idx = 0; idx < NUM_BUTTONS; idx++) {
        bool raw = (digitalRead(BUTTON_PINS[idx]) == LOW);

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

            digitalWrite(LED_PINS[idx], raw ? HIGH : LOW);
            kb_report_pending = true;
        }
    }

    // Send (or re-send) the report. The full keyboard state is sent
    // every time, so even if several changes accumulated the host
    // receives the correct final state once the endpoint is free.
    if (kb_report_pending) {
        if (send_keyboard_report()) {
            kb_report_pending = false;
        }
    }
}

// --------------------------------------------------------
// Read the slider potentiometer and send absolute mouse X.
// ADC 12-bit (0-4095) → HID absolute (0-32767).
// --------------------------------------------------------
static void handle_slider() {
    if (!USBDevice.mounted()) return;

    int32_t acc = 0;
    for (uint8_t i = 0; i < SLIDER_OVERSAMPLE; i++) {
        acc += analogRead(PIN_SLIDER);
    }
    int raw = acc / SLIDER_OVERSAMPLE;

    // Debug: print raw ADC value every ~500ms
    static uint32_t last_dbg = 0;
    if (millis() - last_dbg > 500) {
        last_dbg = millis();
        Serial.print("ADC raw=");
        Serial.println(raw);
    }

    // First reading — just store
    if (slider_smooth < 0) {
        slider_smooth = raw;
        return;
    }

    // EMA low-pass filter to suppress ADC noise / jitter
    slider_smooth = slider_smooth + SLIDER_SMOOTHING * (raw - slider_smooth);

    // Clamp to configured ADC range, then map to HID absolute (0-32767)
    float clamped = (slider_smooth < SLIDER_ADC_MAX) ? slider_smooth : SLIDER_ADC_MAX;
    uint16_t abs_x = (uint16_t)(clamped * (32767.0f / SLIDER_ADC_MAX));
    if (MOUSE_INVERT_X < 0) abs_x = 32767 - abs_x;

    // Only send when movement exceeds dead-zone threshold
    if (abs(abs_x - slider_last_x) < 100) return;
    slider_last_x = abs_x;

    abs_mouse_report_t report = {};
    report.x = abs_x;
    report.y = SLIDER_ABS_Y;
    usb_hid.sendReport(RID_MOUSE, &report, sizeof(report));
}

// --------------------------------------------------------
// P0.09 / P0.10 are wired to the NFC antenna block by default and
// ignore GPIO writes until UICR.NFCPINS is cleared. This is a one-time
// flash write that survives re-flashing; the chip must reset afterward.
// --------------------------------------------------------
static void release_nfc_pins_as_gpio() {
    if ((NRF_UICR->NFCPINS & UICR_NFCPINS_PROTECT_Msk) !=
        (UICR_NFCPINS_PROTECT_NFC << UICR_NFCPINS_PROTECT_Pos)) {
        return;
    }
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}
    NRF_UICR->NFCPINS &= ~UICR_NFCPINS_PROTECT_Msk;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}
    NVIC_SystemReset();
}

// Light each LED in turn at boot so wiring can be checked without pressing anything
static void led_self_test() {
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        digitalWrite(LED_PINS[i], HIGH);
        delay(120);
        digitalWrite(LED_PINS[i], LOW);
    }
}

// --------------------------------------------------------
// Setup
// --------------------------------------------------------
void setup() {
    release_nfc_pins_as_gpio();

    // HID must be registered before the host enumerates us; anything slow
    // (the LED sweep) has to come after this.
    usb_hid.begin();
    if (USBDevice.mounted()) {
        // Host already enumerated without HID — force re-enumeration
        USBDevice.detach();
        delay(10);
        USBDevice.attach();
    }

    // Configure button pins with internal pull-up
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    }

    // Configure LED pins as output (off initially)
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        pinMode(LED_PINS[i], OUTPUT);
        digitalWrite(LED_PINS[i], LOW);
    }

    led_self_test();

    // Configure slider ADC (12-bit). VDD reference makes the reading
    // ratiometric to the pot's supply, so LED-induced rail dips cancel out.
    analogReference(AR_VDD4);
    analogReadResolution(12);
    pinMode(PIN_SLIDER, INPUT);

    // Serial for debugging (optional — open serial monitor to see ADC values)
    Serial.begin(115200);

    // Wait for USB to be ready
    while (!USBDevice.mounted()) {
        delay(1);
    }
}

// --------------------------------------------------------
// Main loop
// --------------------------------------------------------
void loop() {
    // Scan all buttons, send one combined report if any changed
    handle_buttons();

    // Process slider
    handle_slider();

    delay(POLL_INTERVAL_MS);
}
