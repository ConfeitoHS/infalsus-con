#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include "config.h"

using namespace Adafruit_LittleFS_Namespace;

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

// USB HID report descriptor: keyboard + absolute mouse + relative mouse.
// Both mouse reports are always declared; MOUSE_MODE_RELATIVE picks
// which one the slider drives.
uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(RID_KEYBOARD)),
    ABSMOUSE_REPORT_DESC(HID_REPORT_ID(RID_MOUSE)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(RID_MOUSE_REL))
};

// USB HID device
Adafruit_USBD_HID usb_hid(desc_hid_report, sizeof(desc_hid_report),
                           HID_ITF_PROTOCOL_NONE, POLL_INTERVAL_MS, false);

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
static bool     slider_resting = false;
static uint32_t slider_last_move = 0;

// Slider-unit cable presence (detect pin LOW = plugged in)
static bool     slider_connected = false;
static bool     slider_detect_raw = false;
static uint32_t slider_detect_changed = 0;

// Keyboard report pending flag — set when a report needs to be
// (re)sent because the USB endpoint was busy on the previous attempt.
static bool kb_report_pending = false;

// LED brightness (PWM duty), persisted in internal flash
static uint8_t  led_brightness = LED_BRIGHTNESS_DEFAULT;
static const char* BRIGHTNESS_FILE = "/led_brightness";

// Brightness-setup mode can only be entered until this time after boot
static uint32_t brightness_window_end = 0;
static bool     brightness_window_open = true;

static void led_set(uint8_t idx, bool on) {
    analogWrite(LED_PINS[idx], on ? led_brightness : 0);
}

static void leds_all(uint8_t level) {
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        analogWrite(LED_PINS[i], level);
    }
}

static void load_brightness() {
    File f(InternalFS);
    if (!f.open(BRIGHTNESS_FILE, FILE_O_READ)) return;
    uint8_t b;
    if (f.read(&b, 1) == 1) led_brightness = b;
    f.close();
}

static void save_brightness() {
    InternalFS.remove(BRIGHTNESS_FILE);
    File f(InternalFS);
    if (!f.open(BRIGHTNESS_FILE, FILE_O_WRITE)) return;
    f.write(&led_brightness, 1);
    f.close();
}

static bool any_button_down() {
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        if (digitalRead(BUTTON_PINS[i]) == LOW) return true;
    }
    return false;
}

static bool all_buttons_down() {
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        if (digitalRead(BUTTON_PINS[i]) != LOW) return false;
    }
    return true;
}

// Oversampled + EMA-filtered slider reading, clamped to 0..SLIDER_ADC_MAX.
// Returns -1 on the very first call (filter not primed yet).
static int read_slider() {
    int32_t acc = 0;
    for (uint8_t i = 0; i < SLIDER_OVERSAMPLE; i++) {
        acc += analogRead(PIN_SLIDER);
    }
    int raw = acc / SLIDER_OVERSAMPLE;

    if (slider_smooth < 0) {
        slider_smooth = raw;
        return -1;
    }
    float diff  = raw - slider_smooth;
    float alpha = SLIDER_SMOOTHING_MIN + fabsf(diff) * SLIDER_SPEED_GAIN;
    if (alpha > SLIDER_SMOOTHING_MAX) alpha = SLIDER_SMOOTHING_MAX;
    slider_smooth += alpha * diff;

    int v = (int)slider_smooth;
    return (v < SLIDER_ADC_MAX) ? v : SLIDER_ADC_MAX;
}

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

            led_set(idx, raw);
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
// Watch the cable-detect contact. On unplug the ADC input floats,
// so mouse reports are suppressed until a cable is back; LEDs flash
// once on connect and twice on disconnect.
// --------------------------------------------------------
static void handle_slider_detect() {
    bool raw = (digitalRead(PIN_SLIDER_DETECT) == LOW);
    uint32_t now = millis();

    if (raw != slider_detect_raw) {
        slider_detect_raw = raw;
        slider_detect_changed = now;
        return;
    }
    if (raw == slider_connected) return;
    if (now - slider_detect_changed < SLIDER_DETECT_DEBOUNCE_MS) return;

    slider_connected = raw;
    slider_smooth = -1;
    slider_last_x = 0xFFFF;
    slider_resting = false;
    Serial.println(raw ? "slider: connected" : "slider: disconnected");

    uint8_t flashes = raw ? 1 : 2;
    for (uint8_t k = 0; k < flashes; k++) {
        leds_all(LED_SELFTEST_LEVEL);
        delay(80);
        leds_all(0);
        delay(80);
    }
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) led_set(i, btn_pressed[i]);
}

// Send a relative mouse move of `units` HID-absolute units, converted to
// pixels. The fractional remainder is carried to the next call so slow
// moves are not lost to rounding; big moves are split into int8 steps.
static void send_rel_dx(int32_t units, int8_t max_step = 127, uint8_t pace_ms = 0) {
    static float carry = 0;
    float px = units * (MOUSE_REL_PIXELS_PER_TRAVEL / 32767.0f) + carry;
    int32_t dx = (int32_t)px;
    carry = px - dx;
    while (dx != 0) {
        int8_t step = (dx > max_step) ? max_step : (dx < -max_step) ? (int8_t)-max_step : (int8_t)dx;
        // The endpoint may still be busy with a keyboard report; wait for
        // it rather than dropping the move.
        uint32_t t0 = millis();
        while (!usb_hid.ready() && millis() - t0 < 20) delay(1);
        if (!usb_hid.mouseReport(RID_MOUSE_REL, 0, step, 0, 0, 0)) break;
        dx -= step;
        if (pace_ms) delay(pace_ms);
    }
}

// --------------------------------------------------------
// Manual re-centre: tap any RECENTER_CHORD_KEYS (or more) buttons together
// RECENTER_TAPS times within RECENTER_WINDOW_MS. Sends one relative move equal to the slider's
// offset from centre, so a game that has just warped its cursor to the
// centre ends up aligned with where the slider physically is.
// --------------------------------------------------------
static void handle_recenter_chord() {
    static bool     chord_was_down = false;
    static uint8_t  taps = 0;
    static uint32_t first_tap = 0;

    uint8_t down = 0;
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) down += btn_pressed[i];
    bool chord = down >= RECENTER_CHORD_KEYS;
    uint32_t now = millis();

    if (chord && !chord_was_down) {
        if (taps == 0 || now - first_tap > RECENTER_WINDOW_MS) {
            taps = 0;
            first_tap = now;
        }
        taps++;
        if (taps >= RECENTER_TAPS) {
            taps = 0;
            if (slider_last_x != 0xFFFF) {
                Serial.print("recenter: dx units=");
                Serial.println((int32_t)slider_last_x - 16384);
                // Spread the move over many small steps so it looks like a
                // hand movement rather than one huge jump the game may reject
                send_rel_dx((int32_t)slider_last_x - 16384, RECENTER_STEP_PX, RECENTER_STEP_MS);
                for (uint8_t k = 0; k < 2; k++) {
                    leds_all(LED_SELFTEST_LEVEL); delay(60);
                    leds_all(0);                  delay(60);
                }
                for (uint8_t i = 0; i < NUM_BUTTONS; i++) led_set(i, btn_pressed[i]);
            }
        }
    }
    chord_was_down = chord;
}

// --------------------------------------------------------
// Read the slider potentiometer and send absolute mouse X.
// ADC 12-bit (0-4095) → HID absolute (0-32767).
// --------------------------------------------------------
static void handle_slider() {
    if (!USBDevice.mounted() || !slider_connected) return;

    int adc = read_slider();
    if (adc < 0) return;

    // Debug: print filtered ADC value every ~500ms
    static uint32_t last_dbg = 0;
    if (millis() - last_dbg > 500) {
        last_dbg = millis();
        Serial.print("ADC=");
        Serial.println(adc);
    }

    // Map to HID absolute (0-32767)
    uint16_t abs_x = (uint16_t)((uint32_t)adc * 32767UL / SLIDER_ADC_MAX);
    if (MOUSE_INVERT_X < 0) abs_x = 32767 - abs_x;

    // Rest lock: once still, ignore anything smaller than a deliberate move
    uint16_t delta = (abs_x > slider_last_x) ? abs_x - slider_last_x
                                             : slider_last_x - abs_x;
    uint32_t now = millis();
    if (slider_resting) {
        if (delta < SLIDER_WAKE_STEP) return;
        slider_resting = false;
    } else if (delta < SLIDER_MIN_STEP) {
        if (now - slider_last_move >= SLIDER_REST_MS) slider_resting = true;
        return;
    }
    slider_last_move = now;
    uint16_t prev_x = slider_last_x;
    slider_last_x = abs_x;

#if MOUSE_MODE_RELATIVE
    // First sample after (re)connect only sets the reference point
    if (prev_x == 0xFFFF) return;
    send_rel_dx((int32_t)abs_x - (int32_t)prev_x);
#else
    abs_mouse_report_t report = {};
    report.x = abs_x;
    report.y = SLIDER_ABS_Y;
    usb_hid.sendReport(RID_MOUSE, &report, sizeof(report));
#endif
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
        analogWrite(LED_PINS[i], LED_SELFTEST_LEVEL);
        delay(120);
        analogWrite(LED_PINS[i], 0);
    }
}

// --------------------------------------------------------
// Brightness setup: hold all buttons shortly after boot. While they
// stay held the slider sets the LED level live; letting go of any
// button saves it and returns to normal operation.
// --------------------------------------------------------
static void brightness_mode() {
    // The host saw the six-key chord — release everything first
    active_modifier  = 0;
    active_key_count = 0;
    memset(active_keys, 0, sizeof(active_keys));
    send_keyboard_report();

    while (all_buttons_down()) {
        int adc = slider_connected ? read_slider() : -1;
        if (adc >= 0) {
            const float lo = SLIDER_ADC_MAX * SLIDER_USABLE_MIN;
            const float hi = SLIDER_ADC_MAX * SLIDER_USABLE_MAX;
            float pos = (adc - lo) / (hi - lo);
            if (pos < 0.0f) pos = 0.0f;
            if (pos > 1.0f) pos = 1.0f;
            if (MOUSE_INVERT_X < 0) pos = 1.0f - pos;
            uint32_t level = (uint32_t)(pos * 255.0f + 0.5f);
            if (level < LED_BRIGHTNESS_MIN) level = LED_BRIGHTNESS_MIN;
            led_brightness = (uint8_t)level;
            leds_all(led_brightness);
        }
        delay(POLL_INTERVAL_MS);
    }

    save_brightness();

    for (uint8_t k = 0; k < 2; k++) {
        leds_all(0);
        delay(120);
        leds_all(LED_SELFTEST_LEVEL);
        delay(120);
    }
    leds_all(0);

    // Wait for the remaining buttons to be let go so none is sent as a key
    while (any_button_down()) delay(10);
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        btn_pressed[i] = false;
        btn_last_change[i] = millis();
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

    InternalFS.begin();
    load_brightness();

    led_self_test();

    // Configure slider ADC (12-bit). VDD reference makes the reading
    // ratiometric to the pot's supply, so LED-induced rail dips cancel out.
    analogReference(AR_VDD4);
    analogReadResolution(12);
    pinMode(PIN_SLIDER, INPUT);

    pinMode(PIN_SLIDER_DETECT, INPUT_PULLUP);
    delay(5);
    slider_detect_raw = slider_connected = (digitalRead(PIN_SLIDER_DETECT) == LOW);

    // Serial for debugging (optional — open serial monitor to see ADC values)
    Serial.begin(115200);

    // Wait for USB to be ready
    while (!USBDevice.mounted()) {
        delay(1);
    }

    brightness_window_end = millis() + BRIGHTNESS_WINDOW_MS;
}

// --------------------------------------------------------
// Main loop
// --------------------------------------------------------
void loop() {
    if (brightness_window_open) {
        if ((int32_t)(millis() - brightness_window_end) >= 0) {
            brightness_window_open = false;
        } else if (all_buttons_down()) {
            brightness_window_open = false;
            brightness_mode();
            return;
        }
    }

    // Scan all buttons, send one combined report if any changed
    handle_buttons();
    handle_recenter_chord();

    // Process slider
    handle_slider_detect();
    handle_slider();

    delay(POLL_INTERVAL_MS);
}
