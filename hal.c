#include "hal.h"
#include <avr/interrupt.h>
#include <util/delay.h>

// <<< pin definitions >>>
const pin_t PIN_RTEST  = { &DDRD, &PORTD, &PIND, PD2 };
const pin_t PIN_CTEST  = { &DDRD, &PORTD, &PIND, PD3 };
const pin_t PIN_DISCH  = { &DDRD, &PORTD, &PIND, PD4 };
const pin_t LED_R_LOW  = { &DDRB, &PORTB, &PINB, PB0 }; // D8 (DIP14)
const pin_t LED_R_HIGH = { &DDRB, &PORTB, &PINB, PB1 }; // D9 (DIP15)
const pin_t LED_OPEN   = { &DDRB, &PORTB, &PINB, PB2 }; // D10 (DIP16)
const pin_t LED_C_LOW  = { &DDRB, &PORTB, &PINB, PB3 }; // D11 (DIP17)
const pin_t LED_C_HIGH = { &DDRB, &PORTB, &PINB, PB4 }; // D12 (DIP18)
const pin_t LED_DISCH  = { &DDRB, &PORTB, &PINB, PB5 }; // D13 (DIP19)
const pin_t LED_TEST   = { &DDRB, &PORTB, &PINB, PB5 }; // D13 (DIP19)

// <<< pin mode >>>
void pin_mode_output(pin_t p) {
    *p.ddr |= (1 << p.bit);
}

void pin_mode_input(pin_t p) {
    *p.ddr  &= ~(1 << p.bit);
    *p.port &= ~(1 << p.bit); // disable pullup
}

void pin_mode_input_pullup(pin_t p) {
    *p.ddr  &= ~(1 << p.bit);
    *p.port |=  (1 << p.bit); // enable pullup
}

// <<< digital read / write >>>
void digital_write(pin_t p, bool value) {
    if (value) {
        *p.port |=  (1 << p.bit);
    } else {
        *p.port &= ~(1 << p.bit);
    }
}

bool digital_read(pin_t p) {
    return (*p.pin >> p.bit) & 1;
}

// <<< ADC >>>
void adc_init(void) {
    ADMUX  = (1 << REFS0);
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

uint16_t adc_read(uint8_t channel) {
    ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADC;
}

uint16_t analog_read(void) {
    return adc_read(0); // A0 = channel 0
}

// <<< analog comparator >>>
// internal 1.1V bandgap = positive input (ACBG set, AIN0 unused)
// AIN1 (D7, DIP13) = negative input = connect Vx here
// comparator fires when Vx rises above 1.1V (0.248 tau)
// routes comparator output to Timer1 input capture unit
void analog_comp_init(void) {
    DIDR1 |= (1 << AIN1D); // disable digital buffer on AIN1 only (AIN0 unused)
    ACSR &= ~(1 << ACD); // enable comparator
    ACSR |= (1 << ACIC); // route comparator to input capture
    ACSR |= (1 << ACBG); // use internal 1.1V bandgap as positive reference
}

// <<< timer >>>
// /64 prescaler = 4us per tick at 16MHz
// input capture on rising edge (comparator fires when Vx crosses VREF)
// max range: 65535 * 4us = 262ms, enough for 10nF at 1MΩ (50ms)
void timer1_init(void) {
    TCCR1A = 0; // no PWM, normal port operation
    TCCR1B = (1 << CS11) | (1 << CS10) | (1 << ICES1); // /64 prescaler (CS11+CS10), input capture on rising edge (ICES1)
    TCNT1  = 0; // reset counter to 0
}

// <<< delay >>>
void delay(unsigned int ms) {
    while (ms--) {
        _delay_ms(1);
    }
}

// <<< helper functions >>>
void single_led_flash(pin_t led, unsigned int duration_ms) {
    digital_write(led, HIGH);
    delay(duration_ms);
    digital_write(led, LOW);
}

// <<< abs >>>
uint16_t abs_diff(int16_t x) {
    return x < 0 ? -x : x;
}