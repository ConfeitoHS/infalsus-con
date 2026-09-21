// Reset / diagnostic firmware.
//
// Flash this build (pio run -e reset -t upload) to:
//   1. erase the saved settings (LED brightness) from internal flash
//   2. sweep the LEDs forever so wiring can be checked
//   3. print button, slider and cable-detect state over USB serial
//
// It does not touch UICR (the NFC-pin setting stays as GPIO; erasing UICR
// would also clear the USB regulator setting and disable USB).
//
// Afterwards flash the normal firmware again (pio run -t upload).

#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include "config.h"

using namespace Adafruit_LittleFS_Namespace;

static bool fs_formatted = false;

void setup() {
    Serial.begin(115200);

    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        pinMode(BUTTON_PINS[i], INPUT_PULLUP);
        pinMode(LED_PINS[i], OUTPUT);
        digitalWrite(LED_PINS[i], LOW);
    }
    pinMode(PIN_SLIDER_DETECT, INPUT_PULLUP);
    analogReference(AR_VDD4);
    analogReadResolution(12);
    pinMode(PIN_SLIDER, INPUT);

    InternalFS.begin();
    fs_formatted = InternalFS.format();

    // Three quick flashes of every LED = settings erased
    for (uint8_t k = 0; k < 3; k++) {
        for (uint8_t i = 0; i < NUM_BUTTONS; i++) digitalWrite(LED_PINS[i], HIGH);
        delay(100);
        for (uint8_t i = 0; i < NUM_BUTTONS; i++) digitalWrite(LED_PINS[i], LOW);
        delay(100);
    }
}

void loop() {
    static uint8_t  led = 0;
    static uint32_t last_step = 0;
    static uint32_t last_print = 0;
    uint32_t now = millis();

    // LED sweep, 150 ms per LED
    if (now - last_step >= 150) {
        last_step = now;
        digitalWrite(LED_PINS[led], LOW);
        led = (led + 1) % NUM_BUTTONS;
        digitalWrite(LED_PINS[led], HIGH);
    }

    // Status line 4x per second
    if (now - last_print >= 250) {
        last_print = now;
        Serial.print(fs_formatted ? "[settings erased] " : "[format FAILED]  ");
        Serial.print("buttons:");
        for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
            Serial.print(digitalRead(BUTTON_PINS[i]) == LOW ? " 1" : " 0");
        }
        Serial.print("  slider:");
        Serial.print(analogRead(PIN_SLIDER));
        Serial.print("  cable:");
        Serial.println(digitalRead(PIN_SLIDER_DETECT) == LOW ? "in" : "out");
    }

    delay(5);
}
