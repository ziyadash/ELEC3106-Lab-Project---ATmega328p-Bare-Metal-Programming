#include "hal.h"
#include <util/delay.h>

// <<< pin definitions >>>
const pin_t PIN_RTEST  = { &DDRD, &PORTD, &PIND, PD2 }; // D2  (DIP4)
const pin_t PIN_CTEST  = { &DDRD, &PORTD, &PIND, PD3 }; // D3  (DIP5)
const pin_t PIN_DISCH  = { &DDRD, &PORTD, &PIND, PD4 }; // D4  (DIP6)
const pin_t PIN_DTEST  = { &DDRB, &PORTB, &PINB, PB4 }; // D12 (DIP18)
const pin_t PIN_GNDCTL = { &DDRC, &PORTC, &PINC, PC3 }; // A3  (DIP26)

const pin_t LED_R_LOW  = { &DDRD, &PORTD, &PIND, PD5 }; // D5  (DIP11)
const pin_t LED_R_HIGH = { &DDRD, &PORTD, &PIND, PD6 }; // D6  (DIP12)
const pin_t LED_OPEN   = { &DDRB, &PORTB, &PINB, PB0 }; // D8  (DIP14)
const pin_t LED_C_LOW  = { &DDRB, &PORTB, &PINB, PB1 }; // D9  (DIP15)
const pin_t LED_C_HIGH = { &DDRB, &PORTB, &PINB, PB2 }; // D10 (DIP16)
const pin_t LED_DISCH  = { &DDRB, &PORTB, &PINB, PB3 }; // D11 (DIP17)
const pin_t LED_DIODE  = { &DDRC, &PORTC, &PINC, PC1 }; // A1  (DIP25)
const pin_t LED_SHORT  = { &DDRC, &PORTC, &PINC, PC2 }; // A2  (DIP27)

const pin_t LED_TEST   = { &DDRB, &PORTB, &PINB, PB5 }; // D13

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
        *p.port |= (1 << p.bit);
    } else {
        *p.port &= ~(1 << p.bit);
    }
}

bool digital_read(pin_t p) {
    return ((*p.pin >> p.bit) & 1);
}

// <<< ADC >>>
// AVcc reference, right-adjusted result, channel selected in adc_read()
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
    return adc_read(0); // A0 = ADC0
}

// <<< analog comparator >>> 
// positive input  = internal 1.1V bandgap
// negative input  = AIN1 (D7) = Vx
// comparator output is routed internally to Timer1 input capture
void analog_comp_init(void) {
    DIDR1 |= (1 << AIN1D);   // disable digital buffer on AIN1
    ACSR &= ~(1 << ACD);     // enable comparator
    ACSR &= ~(1 << ACIE);    // comparator interrupt disabled
    ACSR |= (1 << ACIC);     // route comparator to Timer1 input capture
    ACSR |= (1 << ACBG);     // use internal 1.1V bandgap as positive input
}

// <<< timer >>> 
// /64 prescaler -> 4us per tick at 16MHz
// capture on falling edge:
//
// comparator output is high while 1.1V > Vx
// when Vx rises past 1.1V, comparator output goes low
// so the threshold crossing produces a falling edge
void timer1_init(void) {
    TCCR1A = 0;
    TCCR1B = 0;

    TCCR1B |= (1 << CS11) | (1 << CS10); // /64 prescaler
    TCCR1B &= ~(1 << ICES1);             // capture on falling edge

    TCNT1 = 0;
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
    return (x < 0) ? (uint16_t)(-x) : (uint16_t)x;
}