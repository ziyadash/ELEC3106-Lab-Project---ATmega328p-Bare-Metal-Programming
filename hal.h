#ifndef HAL_H
#define HAL_H

#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>

// high and low
#define HIGH 1
#define LOW 0

// ADC thresholds
#define ADC_OPEN 50
#define ADC_NOT_RESISTOR 800
#define ADC_R_SPLIT 385
#define ADC_CHARGED 950

// Capacitor thresholds TODO RECALCULATE THESE
#define T_C_LOW_MS 176 // not needed..?
#define T_C_SPLIT_MS 528
#define T_TIMEOUT_MS 3000

// Stable measurement constants, we need to make 5 readings with an ADC tolerance
// of 10 to confirm that a voltage is 'constant' at some value
#define STABLE_READINGS 5
#define STABLE_TOLERANCE 10

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
extern const pin_t PIN_RTEST;
extern const pin_t PIN_DISCH;
extern const pin_t PIN_CTEST;
extern const pin_t LED_R_LOW;
extern const pin_t LED_R_HIGH;
extern const pin_t LED_OPEN;
extern const pin_t LED_C_LOW;
extern const pin_t LED_C_HIGH;
extern const pin_t LED_DISCH;

// temporary test pin - onboard LED on D13
extern const pin_t LED_TEST;

// <<< pin mode >>>
void pin_mode_output(pin_t p);
void pin_mode_input(pin_t p);
void pin_mode_input_pullup(pin_t p);

// <<< digital read / write >>>
void digital_write(pin_t p, bool value);
bool digital_read(pin_t p);

// <<< ADC >>>
void adc_init(void);
uint16_t adc_read(uint8_t channel);
uint16_t analog_read(void);

// <<< timer / millis >>>
void timer1_init(void);
uint32_t millis(void);

// thin wrapper over internal _delay_ms function,
// which is a busy loop that blocks the CPU for the specified time
void delay(unsigned int ms);

// <<< high level helper functions >>>
void leds_off(void);
void single_led_flash(pin_t led_pin);
void discharge(void);
void show_open(void);

// renamed from abs to avoid colliding with the standard library function name
uint16_t abs_diff(int16_t x);

#endif