#ifndef HAL_H
#define HAL_H

#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>

// high and low
#define HIGH 1
#define LOW  0

// ---------------------------------------------------------------------------------------
// abstraction layer over AVR register manipulation
// ---------------------------------------------------------------------------------------

// <<< pin type >>>
typedef struct {
    volatile uint8_t *ddr;   // data direction register
    volatile uint8_t *port;  // output / pullup register
    volatile uint8_t *pin;   // input register
    uint8_t bit;             // bit within the register
} pin_t;

// <<< pin definitions >>>
extern const pin_t PIN_RTEST;   // D2  - resistor test drive through 5kΩ
extern const pin_t PIN_CTEST;   // D3  - capacitor test drive through 1MΩ
extern const pin_t PIN_DISCH;   // D4  - discharge transistor drive
extern const pin_t PIN_DTEST;  // D12 - diode test drive, second side of DUT

extern const pin_t LED_R_LOW;   // D5  - LED red    -> resistor 1-3kΩ
extern const pin_t LED_R_HIGH;  // D6  - LED blue   -> resistor 3-10kΩ
extern const pin_t LED_OPEN;    // D8  - LED green  -> open circuit
extern const pin_t LED_C_LOW;   // D9  - LED yellow -> cap 1-3nF
extern const pin_t LED_C_HIGH;  // D10 - LED white  -> cap 3-10nF
extern const pin_t LED_DISCH;   // D11 - LED orange -> discharging
extern const pin_t LED_DIODE;   // A1  - LED for diode detection
extern const pin_t LED_SHORT;   // A2 - LED for short circuit

extern const pin_t LED_TEST;    // D13 - onboard LED, used for testing

// comparator / ADC notes:
// AIN0 (D6)  - unused externally, internal 1.1V bandgap is used instead
// AIN1 (D7)  - comparator negative input, connect Vx here
// ADC0 (A0)  - ADC reads Vx for resistor test

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

// <<< analog comparator >>>
// internal 1.1V bandgap = positive comparator input
// AIN1 (D7) = negative comparator input
// comparator output is routed internally to Timer1 input capture
void analog_comp_init(void);

// <<< timer >>> 
// Timer1 is used for capacitor timing
// /64 prescaler = 4us per tick at 16MHz
// capture on falling edge because comparator output goes high -> low
// when Vx rises past the 1.1V bandgap threshold
void timer1_init(void);

// <<< delay >>>
void delay(unsigned int ms);

// <<< helper functions >>>
void single_led_flash(pin_t led, unsigned int duration_ms);

// <<< abs >>>
uint16_t abs_diff(int16_t x);

#endif