#ifndef HAL_H
#define HAL_H

#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>

// high and low
#define HIGH 1
#define LOW 0

// ---------------------------------------------------------------------------------------
// the goal is to define an abstraction layer for the real low level register manipulation
// and make an interface similar to the Arduino API, so that the main logic
// can be easily translated.
// ---------------------------------------------------------------------------------------

// <<< pin type >>>
// neat little struct to bundle all the registers and bit info for a pin together
// based on my notes about the pinout
typedef struct {
    volatile uint8_t *ddr;  // data direction register
    volatile uint8_t *port; // output / pullup register
    volatile uint8_t *pin;  // input register
    uint8_t bit;            // which bit within the register
} pin_t;

// <<< pin definitions >>>
extern const pin_t PIN_RTEST;  // D2  (DIP4)  - resistor test drive through 5kΩ
extern const pin_t PIN_CTEST;  // D3  (DIP5)  - capacitor test drive through 1MΩ
extern const pin_t PIN_DISCH;  // D4  (DIP6)  - discharge pin, drives BC548 base via 1kΩ
extern const pin_t LED_R_LOW;  // D8  (DIP14) - LED red    → resistor 1-3kΩ
extern const pin_t LED_R_HIGH; // D9  (DIP15) - LED blue   → resistor 3-10kΩ
extern const pin_t LED_OPEN;   // D10 (DIP16) - LED green  → open circuit
extern const pin_t LED_C_LOW;  // D11 (DIP17) - LED yellow → cap 1-3nF
extern const pin_t LED_C_HIGH; // D12 (DIP18) - LED white  → cap 3-10nF
extern const pin_t LED_DISCH;  // D13 (DIP19) - LED orange → discharging
extern const pin_t LED_TEST;   // D13 (DIP19) - onboard LED, used for testing
// AIN0 D6 (DIP12) - unused, internal 1.1V bandgap used as positive reference
// AIN1 D7 (DIP13) - on-chip comparator negative input, connect Vx here
// A0   (DIP23) - ADC reads Vx for resistor test

// <<< pin mode >>>
void pin_mode_output(pin_t p);
void pin_mode_input(pin_t p);
void pin_mode_input_pullup(pin_t p);

// <<< digital read / write >>>
void digital_write(pin_t p, bool value);
bool digital_read(pin_t p);

// <<< ADC >>>
// A0 (DIP23) reads Vx for resistor and open circuit tests
void adc_init(void);
uint16_t adc_read(uint8_t channel);
uint16_t analog_read(void);

// <<< analog comparator >>>
// AIN0 = D6 = VREF, AIN1 = D7 = Vx
// routes comparator output to Timer1 input capture unit
void analog_comp_init(void);

// <<< timer >>>
// /64 prescaler, 4us per tick, input capture on rising edge
// used for capacitor timing via on-chip analog comparator
void timer1_init(void);

// <<< delay >>>
// thin wrapper over _delay_ms, which is a busy loop calibrated against F_CPU
void delay(unsigned int ms);

// <<< helper functions >>>
void single_led_flash(pin_t led, unsigned int duration_ms);

// <<< abs >>>
// renamed from abs to avoid colliding with the standard library
uint16_t abs_diff(int16_t x);

#endif