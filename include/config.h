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
//     GND ┤      RST ├ ← reset tact switch → GND
//     GND ┤      VCC ├ 3.3V out
//   P0.17 ┤ D2   D17 ├ P0.31  ← slider cable detect
//   P0.20 ┤ D3   D16 ├ P0.29  (A1) ← slider
//   P0.22 ┤ D4   D15 ├ P0.02  (A0) ← SW1
//   P0.24 ┤ D5   D14 ├ P1.15  ← SW2
//   P1.00 ┤ D6   D13 ├ P1.13  ← SW3
//   P0.11 ┤ D7   D12 ├ P1.11  ← SW4
//   P1.04 ┤ D8   D11 ├ P0.10  ← SW5
//   P1.06 ┤ D9   D10 ├ P0.09  ← SW6
//   LED1..LED6 on D4..D9 (left side)
//         └──────────┘
// ============================================================

// ============================================================
// Pin numbers by board. Wiring is by PHYSICAL Pro Micro position, so the
// same case/harness fits either board; only the GPIO numbers differ.
//
//   Pro Micro position :  RP2040 Pro Micro  |  nice!nano (Arduino idx / nRF)
//   left  D2..D9       :  GP2..GP9          |  2..9  / P0.17 P0.20 P0.22 P0.24 P1.00 P0.11 P1.04 P1.06
//   right A3 A2 A1 A0  :  GP29 28 27 26     |  17 16 15 14 / P0.31 P0.29 P0.02 P1.15
//   right D15 D14      :  GP22 GP20         |  13 12 / P1.13 P1.11
//   right D16 D10      :  GP23 GP21         |  11 10 / P0.10 P0.09
// ============================================================
#if defined(ARDUINO_ARCH_RP2040)
  #define PIN_SLIDER          28  // A2 position — GP28 / ADC2
  #define PIN_SLIDER_DETECT   29  // A3 position — GP29
#else
  // Slider (potentiometer) wiper — board label 029 (P0.29, AIN5)
  #define PIN_SLIDER          A1
  // Slider-unit cable detect — board label 031 (P0.31). Wired to the 2nd
  // ring contact of the 4-pole jack; the plug's sleeve shorts it to GND
  // whenever a cable is inserted, so LOW = connected.
  #define PIN_SLIDER_DETECT   17
#endif

#if defined(ARDUINO_ARCH_RP2040)
  #if WIRING_V1
  // ---- Wiring v1 on RP2040: buttons LEFT (D2-D7), LEDs RIGHT
  #define PIN_BTN_1  2
  #define PIN_BTN_2  3
  #define PIN_BTN_3  4
  #define PIN_BTN_4  5
  #define PIN_BTN_5  6
  #define PIN_BTN_6  7
  #define PIN_LED_1  21  // D10 position
  #define PIN_LED_2  23  // D16 position
  #define PIN_LED_3  20  // D14 position
  #define PIN_LED_4  22  // D15 position
  #define PIN_LED_5  26  // A0 position
  #define PIN_LED_6  27  // A1 position
  #else
  // ---- Wiring v2 on RP2040: buttons RIGHT, LEDs LEFT (D4-D9)
  #define PIN_BTN_1  27  // A1 position
  #define PIN_BTN_2  26  // A0 position
  #define PIN_BTN_3  22  // D15 position
  #define PIN_BTN_4  20  // D14 position
  #define PIN_BTN_5  23  // D16 position
  #define PIN_BTN_6  21  // D10 position
  #define PIN_LED_1  4
  #define PIN_LED_2  5
  #define PIN_LED_3  6
  #define PIN_LED_4  7
  #define PIN_LED_5  8
  #define PIN_LED_6  9
  #endif
#elif WIRING_V1
// ---- Wiring v1 (older builds): buttons on the LEFT column, LEDs on the RIGHT
// Build with:  pio run -e nrf52840_v1 [-t upload]
#define PIN_BTN_1       2   // D2  — P0.17
#define PIN_BTN_2       3   // D3  — P0.20
#define PIN_BTN_3       4   // D4  — P0.22
#define PIN_BTN_4       5   // D5  — P0.24
#define PIN_BTN_5       6   // D6  — P1.00
#define PIN_BTN_6       7   // D7  — P0.11

#define PIN_LED_1       10  // D10 — P0.09  (NFC pin; main.cpp clears UICR.NFCPINS)
#define PIN_LED_2       11  // D11 — P0.10  (NFC pin)
#define PIN_LED_3       12  // D12 — P1.11
#define PIN_LED_4       13  // D13 — P1.13
#define PIN_LED_5       14  // D14 — P1.15
#define PIN_LED_6       15  // D15 — P0.02
#else
// ---- Wiring v2 (current): buttons on the RIGHT column, LEDs on the LEFT
// Button pins (active LOW with internal pull-up), right side, top to bottom
#define PIN_BTN_1       15  // A0  — P0.02
#define PIN_BTN_2       14  // D14 — P1.15
#define PIN_BTN_3       13  // D13 — P1.13
#define PIN_BTN_4       12  // D12 — P1.11
#define PIN_BTN_5       11  // D11 — P0.10
#define PIN_BTN_6       10  // D10 — P0.09  (NFC pin; main.cpp clears UICR.NFCPINS)

// LED pins (active HIGH, one per button), left side, D4..D9 top to bottom.
// Driven through an NPN transistor so LED current comes from VCC:
//   pin → 4.7kΩ → 2N2222 base; emitter → GND;
//   collector → LED(-) ; LED(+) → 220Ω → 3.3V
// (Driving the LED straight from the pin also works with a 1kΩ resistor,
// but nRF52840 GPIO is limited to ~2mA/pin, ~15mA total. RP2040 GPIO can
// drive 220Ω LEDs directly.)
#define PIN_LED_1       4   // D4 — P0.22  (for Button 1)
#define PIN_LED_2       5   // D5 — P0.24  (for Button 2)
#define PIN_LED_3       6   // D6 — P1.00  (for Button 3)
#define PIN_LED_4       7   // D7 — P0.11  (for Button 4)
#define PIN_LED_5       8   // D8 — P1.04  (for Button 5)
#define PIN_LED_6       9   // D9 — P1.06  (for Button 6)
#endif

#define NUM_BUTTONS     6

static const uint8_t BUTTON_PINS[NUM_BUTTONS] = {
    PIN_BTN_1, PIN_BTN_2, PIN_BTN_3,
    PIN_BTN_4, PIN_BTN_5, PIN_BTN_6
};

static const uint8_t LED_PINS[NUM_BUTTONS] = {
    PIN_LED_1, PIN_LED_2, PIN_LED_3,
    PIN_LED_4, PIN_LED_5, PIN_LED_6
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
#define RID_MOUSE       2   // absolute
#define RID_MOUSE_REL   3   // relative

// ============================================================
// Tuning Parameters
// ============================================================

// Button debounce time (ms)
#define DEBOUNCE_MS         20

// Slider cable plug/unplug debounce (ms)
#define SLIDER_DETECT_DEBOUNCE_MS   50

// Invert mouse X direction (set to -1 to reverse, 1 for normal)
#define MOUSE_INVERT_X      -1

// 1 = send relative mouse movement (Δx), like a normal mouse. Games that
//     warp the cursor to the centre at song start and then track movement
//     (in falsus) need this: the game's centre becomes the reference and
//     no manual re-centring is needed.
// 0 = send the slider's absolute screen position.
#define MOUSE_MODE_RELATIVE 1

// Pixels the full (electrical) slider travel corresponds to. Used by
// relative mode and by the manual re-centre move. 3840: the ~60mm reachable
// in the housing covers one 1080p screen width at ~1 ADC step per pixel;
// going much higher makes the cursor move in 2px steps.
#define MOUSE_REL_PIXELS_PER_TRAVEL 3840.0f

// Manual re-centre: tap the buttons in RECENTER_CHORD_MASK together
// RECENTER_TAPS times within RECENTER_WINDOW_MS to send one relative move
// equal to the slider's offset from centre. Use right after the game has
// warped its cursor to the centre so its position matches the slider.
// Mask bit i = button i+1. All of CHORD_MASK plus at least one of ANY_MASK
// must be down: 0b011110 = A S D F, 0b100001 = Shift or Space.
#define RECENTER_CHORD_MASK     0b011110
#define RECENTER_CHORD_ANY_MASK 0b100001
#define RECENTER_TAPS       4
#define RECENTER_WINDOW_MS  500

// The re-centre move is sent as small steps spaced out in time, like a
// hand movement, so games that clamp or discard huge deltas accept it.
// 8px every 2ms = ~1000px in 250ms.
#define RECENTER_STEP_PX    8
#define RECENTER_STEP_MS    2

// Absolute mode only: fixed Y position (0-32767). Center = 16384.
#define SLIDER_ABS_Y        16384

// Slider ADC range — with the VDD reference the pot's top end reads
// near full scale. Lower this if the cursor never reaches the screen edge
// (check Serial debug output for the actual maximum).
#define SLIDER_ADC_MAX      4060

// Samples averaged per poll before the EMA filter
#define SLIDER_OVERSAMPLE   16

// Adaptive EMA: the smoothing factor scales with how fast the slider is
// moving, so it filters hard while nearly still and follows quickly when
// moved. alpha = MIN + |raw - smooth| * GAIN, capped at MAX.
#define SLIDER_SMOOTHING_MIN    0.08f
#define SLIDER_SMOOTHING_MAX    0.6f
#define SLIDER_SPEED_GAIN       0.02f

// Smallest change (HID units, 0-32767) sent while the slider is moving.
// 32767 units span the screen, so 16 ≈ 1px on a 1920px display.
#define SLIDER_MIN_STEP     16

// After SLIDER_REST_MS without a change ≥ SLIDER_MIN_STEP the slider is
// considered at rest and nothing is sent until it moves ≥ SLIDER_WAKE_STEP
// (64 ≈ 4px) — this is what stops the cursor twitching when untouched.
#define SLIDER_WAKE_STEP    64
#define SLIDER_REST_MS      150

// Input scan period and USB HID polling interval (ms). 1 = 1000Hz,
// the fastest a full-speed USB device can be polled.
#define POLL_INTERVAL_MS    1

// ============================================================
// LED brightness
// ============================================================

// PWM level (0-255) used until one is saved from brightness-setup mode
#define LED_BRIGHTNESS_DEFAULT  255

// Lowest level the slider can set (0 = fully off)
#define LED_BRIGHTNESS_MIN      0

// Level used by the boot self-test sweep, independent of the saved value
#define LED_SELFTEST_LEVEL      128

// 1 if the LEDs light when the pin is LOW (LED between 3.3V and the pin,
// or a PNP driver). With the reference wiring (NPN, LED to 3.3V via 220Ω)
// leave this at 0. Symptom of a mismatch: every LED on at boot and the
// pressed button's LED going dark.
#define LED_ACTIVE_LOW          0

// Fraction of the pot's electrical travel the knob can actually reach
// (100mm pot in a 60mm housing). Brightness setup maps this span to
// MIN..255; positions outside it clamp to the nearest end.
#define SLIDER_USABLE_MIN       0.25f
#define SLIDER_USABLE_MAX       0.75f

// Hold all buttons within this long after USB connects to enter
// brightness-setup mode: while held the slider sets the level,
// releasing saves and exits
#define BRIGHTNESS_WINDOW_MS    10000

// Give up on brightness-setup mode after this long even if the buttons
// still read pressed (protects against buttons stuck low)
#define BRIGHTNESS_MODE_TIMEOUT_MS  30000
