#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include "config.h"

// ---- Platform layer: settings storage in internal flash ---------------
#if defined(ARDUINO_ARCH_RP2040)
  #include <LittleFS.h>
  static void settings_begin() { LittleFS.begin(); }
  static bool settings_read(const char* path, void* buf, size_t len) {
      File f = LittleFS.open(path, "r");
      if (!f) return false;
      bool ok = f.read((uint8_t*)buf, len) == (int)len;
      f.close();
      return ok;
  }
  static void settings_write(const char* path, const void* buf, size_t len) {
      File f = LittleFS.open(path, "w");
      if (!f) return;
      f.write((const uint8_t*)buf, len);
      f.close();
  }
#else
  #include <Adafruit_LittleFS.h>
  #include <InternalFileSystem.h>
  using namespace Adafruit_LittleFS_Namespace;
  static void settings_begin() { InternalFS.begin(); }
  static bool settings_read(const char* path, void* buf, size_t len) {
      File f(InternalFS);
      if (!f.open(path, FILE_O_READ)) return false;
      bool ok = f.read(buf, len) == (int)len;
      f.close();
      return ok;
  }
  static void settings_write(const char* path, const void* buf, size_t len) {
      InternalFS.remove(path);   // FILE_O_WRITE appends, so start fresh
      File f(InternalFS);
      if (!f.open(path, FILE_O_WRITE)) return;
      f.write((const uint8_t*)buf, len);
      f.close();
  }
#endif

// ---- User settings (persisted as one record) ----------------------------
// Adding a field: append it, bump SETTINGS_VERSION and give it a default in
// settings_defaults(); an older record is then ignored and defaults apply.
#define SETTINGS_MAGIC    0x49   // 'I'
#define SETTINGS_VERSION  1
typedef struct __attribute__((packed)) {
    uint8_t  magic;
    uint8_t  version;
    uint8_t  brightness;     // LED PWM level 0-255
    uint8_t  reverse;        // 1 = flip the built-in slider direction
    uint16_t px_per_travel;  // pixels for the full slider travel
} settings_t;

static const char* SETTINGS_FILE   = "/settings";
static const char* LEGACY_BRIGHTNESS_FILE = "/led_brightness";  // pre-1.0 builds

static settings_t cfg;        // live values
static settings_t cfg_saved;  // what is in flash (to report unsaved changes)

static void settings_defaults(settings_t& s) {
    s.magic         = SETTINGS_MAGIC;
    s.version       = SETTINGS_VERSION;
    s.brightness    = LED_BRIGHTNESS_DEFAULT;
    s.reverse       = 0;
    s.px_per_travel = MOUSE_REL_PIXELS_PER_TRAVEL;
}

static bool settings_valid(const settings_t& s) {
    return s.magic == SETTINGS_MAGIC && s.version == SETTINGS_VERSION &&
           s.reverse <= 1 &&
           s.px_per_travel >= PX_PER_TRAVEL_MIN && s.px_per_travel <= PX_PER_TRAVEL_MAX;
}

static void load_settings() {
    settings_t s;
    if (!settings_read(SETTINGS_FILE, &s, sizeof(s)) || !settings_valid(s)) {
        settings_defaults(s);
        uint8_t b;   // keep a brightness saved by an older firmware
        if (settings_read(LEGACY_BRIGHTNESS_FILE, &b, 1)) s.brightness = b;
    }
    cfg = cfg_saved = s;
}

static void save_settings() {
    settings_write(SETTINGS_FILE, &cfg, sizeof(cfg));
    cfg_saved = cfg;
}

static bool settings_dirty() { return memcmp(&cfg, &cfg_saved, sizeof(cfg)) != 0; }

// Effective slider direction: the wiring's built-in direction, optionally flipped
static inline bool invert_x() { return (MOUSE_INVERT_X < 0) != (cfg.reverse != 0); }

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

// All LEDs are shown at the brightness level until this time (serial preview)
static uint32_t led_preview_until = 0;

// Brightness-setup mode can only be entered until this time after boot
static uint32_t brightness_window_end = 0;
static bool     brightness_window_open = true;

static inline uint8_t led_duty(uint8_t level) {
    return LED_ACTIVE_LOW ? (uint8_t)(255 - level) : level;
}

static void led_set(uint8_t idx, bool on) {
    analogWrite(LED_PINS[idx], led_duty(on ? cfg.brightness : 0));
}

static void leds_all(uint8_t level) {
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        analogWrite(LED_PINS[i], led_duty(level));
    }
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
    if (!TinyUSBDevice.mounted()) return;

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
    float px = units * (cfg.px_per_travel / 32767.0f) + carry;
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
// Manual re-centre: tap all RECENTER_CHORD_MASK buttons plus at least one
// RECENTER_CHORD_ANY_MASK button together RECENTER_TAPS times within
// RECENTER_WINDOW_MS. Sends one relative move equal to the slider's
// offset from centre, so a game that has just warped its cursor to the
// centre ends up aligned with where the slider physically is.
// --------------------------------------------------------
static void handle_recenter_chord() {
    static bool     chord_was_down = false;
    static uint8_t  taps = 0;
    static uint32_t first_tap = 0;

    uint8_t down = 0;
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) if (btn_pressed[i]) down |= (1 << i);
    bool chord = (down & RECENTER_CHORD_MASK) == RECENTER_CHORD_MASK &&
                 (down & RECENTER_CHORD_ANY_MASK) != 0;
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
    if (!TinyUSBDevice.mounted() || !slider_connected) return;

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
    if (invert_x()) abs_x = 32767 - abs_x;

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

// Light each LED in turn at boot so wiring can be checked without pressing anything
static void led_self_test() {
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        analogWrite(LED_PINS[i], led_duty(LED_SELFTEST_LEVEL));
        delay(120);
        analogWrite(LED_PINS[i], led_duty(0));
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

    // Bail out after a while so a board with buttons stuck low (e.g. wired
    // to the NC contact) does not sit here forever with every LED lit.
    uint32_t entered = millis();
    while (all_buttons_down() && millis() - entered < BRIGHTNESS_MODE_TIMEOUT_MS) {
        int adc = slider_connected ? read_slider() : -1;
        if (adc >= 0) {
            const float lo = SLIDER_ADC_MAX * SLIDER_USABLE_MIN;
            const float hi = SLIDER_ADC_MAX * SLIDER_USABLE_MAX;
            float pos = (adc - lo) / (hi - lo);
            if (pos < 0.0f) pos = 0.0f;
            if (pos > 1.0f) pos = 1.0f;
            if (invert_x()) pos = 1.0f - pos;
            uint32_t level = (uint32_t)(pos * 255.0f + 0.5f);
            if (level < LED_BRIGHTNESS_MIN) level = LED_BRIGHTNESS_MIN;
            cfg.brightness = (uint8_t)level;
            leds_all(cfg.brightness);
        }
        delay(POLL_INTERVAL_MS);
    }

    // Commit only the brightness; other unsaved (serial) changes stay live
    // but unsaved
    settings_t live = cfg;
    cfg = cfg_saved;
    cfg.brightness = live.brightness;
    save_settings();
    cfg = live;

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
// Serial configuration protocol (USB CDC, 115200, one command per line).
// Every reply is a single JSON line starting with '{'; other lines on the
// port (debug output) do not start with '{' and can be ignored.
//
//   info                     identity, capabilities, limits, defaults, settings
//   get                      current settings
//   set brightness <0-255>   LED level (all LEDs preview it briefly)
//   set px <min-max>         pixels for the full slider travel
//   set reverse <0|1>        flip the slider direction
//   save                     write current settings to flash
//   revert                   reload the settings saved in flash
//   defaults                 load factory defaults (not saved until "save")
//
// "set" applies immediately but is lost on unplug unless "save" follows.
// --------------------------------------------------------
static void print_settings_json(const settings_t& s) {
    Serial.print("{\"brightness\":");  Serial.print(s.brightness);
    Serial.print(",\"px\":");          Serial.print(s.px_per_travel);
    Serial.print(",\"reverse\":");     Serial.print(s.reverse);
    Serial.print("}");
}

static void reply_settings(const char* type) {
    Serial.print("{\"ok\":true,\"type\":\"");
    Serial.print(type);
    Serial.print("\",\"settings\":");
    print_settings_json(cfg);
    Serial.print(",\"dirty\":");
    Serial.print(settings_dirty() ? "true" : "false");
    Serial.println("}");
}

static void reply_error(const char* msg) {
    Serial.print("{\"ok\":false,\"error\":\"");
    Serial.print(msg);
    Serial.println("\"}");
}

static void reply_info() {
    settings_t d;
    settings_defaults(d);
    Serial.print("{\"ok\":true,\"type\":\"info\"");
    Serial.print(",\"name\":\"" FW_NAME "\"");
    Serial.print(",\"fw\":\"" FW_VERSION "\"");
    Serial.print(",\"proto\":");  Serial.print(PROTO_VERSION);
    Serial.print(",\"board\":\"" BOARD_ID "\"");
    Serial.print(",\"hw\":\"" HW_REV "\"");
    Serial.print(",\"wiring\":\"" WIRING_ID "\"");
    Serial.print(",\"slider\":\"" SLIDER_TYPE "\"");
    Serial.print(",\"buttons\":"); Serial.print(NUM_BUTTONS);
    Serial.print(",\"caps\":[\"brightness\",\"px\",\"reverse\"]");
    Serial.print(",\"limits\":{\"brightness\":[0,255],\"px\":[");
    Serial.print(PX_PER_TRAVEL_MIN); Serial.print(","); Serial.print(PX_PER_TRAVEL_MAX);
    Serial.print("],\"reverse\":[0,1]}");
    Serial.print(",\"defaults\":"); print_settings_json(d);
    Serial.print(",\"settings\":"); print_settings_json(cfg);
    Serial.print(",\"dirty\":");    Serial.print(settings_dirty() ? "true" : "false");
    Serial.println("}");
}

static bool parse_long(const char* s, long* out) {
    if (!s || !*s) return false;
    char* end;
    long v = strtol(s, &end, 10);
    if (*end != '\0') return false;
    *out = v;
    return true;
}

static void restore_button_leds() {
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) led_set(i, btn_pressed[i]);
}

static void run_command(char* line) {
    char* cmd = strtok(line, " \t");
    if (!cmd) return;
    char* key = strtok(NULL, " \t");
    char* val = strtok(NULL, " \t");

    if (!strcmp(cmd, "info")) { reply_info(); return; }
    if (!strcmp(cmd, "get"))  { reply_settings("settings"); return; }
    if (!strcmp(cmd, "save")) { save_settings(); reply_settings("saved"); return; }
    if (!strcmp(cmd, "revert") || !strcmp(cmd, "defaults")) {
        if (cmd[0] == 'r') cfg = cfg_saved;
        else               settings_defaults(cfg);
        slider_last_x = 0xFFFF;   // direction may have changed
        restore_button_leds();
        reply_settings("settings");
        return;
    }
    if (!strcmp(cmd, "set")) {
        long v;
        if (!key || !parse_long(val, &v)) { reply_error("usage: set <key> <number>"); return; }
        if (!strcmp(key, "brightness")) {
            if (v < 0 || v > 255) { reply_error("brightness out of range"); return; }
            cfg.brightness = (uint8_t)v;
            leds_all(cfg.brightness);
            led_preview_until = millis() + LED_PREVIEW_MS;
            if (!led_preview_until) led_preview_until = 1;
        } else if (!strcmp(key, "px")) {
            if (v < PX_PER_TRAVEL_MIN || v > PX_PER_TRAVEL_MAX) { reply_error("px out of range"); return; }
            cfg.px_per_travel = (uint16_t)v;
        } else if (!strcmp(key, "reverse")) {
            if (v != 0 && v != 1) { reply_error("reverse must be 0 or 1"); return; }
            cfg.reverse = (uint8_t)v;
            slider_last_x = 0xFFFF;   // new direction: take a fresh reference
        } else {
            reply_error("unknown setting");
            return;
        }
        reply_settings("settings");
        return;
    }
    reply_error("unknown command");
}

static void handle_serial() {
    static char    buf[64];
    static uint8_t len = 0;
    static bool    overflow = false;

    while (Serial.available() > 0) {
        int c = Serial.read();
        if (c < 0) break;
        if (c == '\r') continue;
        if (c == '\n') {
            buf[len] = '\0';
            if (overflow) reply_error("line too long");
            else run_command(buf);
            len = 0;
            overflow = false;
        } else if (len < sizeof(buf) - 1) {
            buf[len++] = (char)c;
        } else {
            overflow = true;
        }
    }

    // End of the brightness preview: back to showing pressed buttons
    if (led_preview_until && (int32_t)(millis() - led_preview_until) >= 0) {
        led_preview_until = 0;
        restore_button_leds();
    }
}

// --------------------------------------------------------
// nRF52840 only: P0.09 / P0.10 are wired to the NFC antenna block by
// default and ignore GPIO until UICR.NFCPINS is cleared. One-time flash
// write that survives re-flashing; the chip must reset afterward.
// --------------------------------------------------------
static void release_nfc_pins_as_gpio() {
#if defined(NRF52840_XXAA)
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
#endif
}

// --------------------------------------------------------
// Setup
// --------------------------------------------------------
void setup() {
    release_nfc_pins_as_gpio();

    // HID must be registered before the host enumerates us; anything slow
    // (the LED sweep) has to come after this.
    usb_hid.begin();
    if (TinyUSBDevice.mounted()) {
        // Host already enumerated without HID — force re-enumeration
        TinyUSBDevice.detach();
        delay(10);
        TinyUSBDevice.attach();
    }

    // Configure button pins with internal pull-up
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    }

    // Configure LED pins as output (off initially)
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        pinMode(LED_PINS[i], OUTPUT);
        digitalWrite(LED_PINS[i], LED_ACTIVE_LOW ? HIGH : LOW);
#if defined(ARDUINO_ARCH_RP2040)
        // Default drive is 4mA; LEDs are wired straight to the pin via 220Ω
        gpio_set_drive_strength(LED_PINS[i], GPIO_DRIVE_STRENGTH_12MA);
#endif
    }

    settings_begin();
    load_settings();

    led_self_test();

    // Configure slider ADC (12-bit). VDD reference makes the reading
    // ratiometric to the pot's supply, so LED-induced rail dips cancel out.
#if !defined(ARDUINO_ARCH_RP2040)
    analogReference(AR_VDD4);
#endif
    analogReadResolution(12);
    pinMode(PIN_SLIDER, INPUT);

    pinMode(PIN_SLIDER_DETECT, INPUT_PULLUP);
    delay(5);
    slider_detect_raw = slider_connected = (digitalRead(PIN_SLIDER_DETECT) == LOW);

    // Serial for debugging (optional — open serial monitor to see ADC values)
    Serial.begin(115200);

    // Wait for USB to be ready
    while (!TinyUSBDevice.mounted()) {
        delay(1);
    }

    brightness_window_end = millis() + BRIGHTNESS_WINDOW_MS;
}

// --------------------------------------------------------
// Main loop
// --------------------------------------------------------
void loop() {
    if (brightness_window_open) {
        // Arm only once the buttons have been seen released after boot, so
        // buttons that read pressed permanently cannot trap us in setup mode.
        static bool armed = false;
        if (!armed && !any_button_down()) armed = true;

        if ((int32_t)(millis() - brightness_window_end) >= 0) {
            brightness_window_open = false;
        } else if (armed && all_buttons_down()) {
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

    // Configuration commands from the host
    handle_serial();

    delay(POLL_INTERVAL_MS);
}
