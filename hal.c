#include "hal.h"
#include <avr/interrupt.h>
#include <util/delay.h>

// <<< pin definitions >>>

// temporary test pin - onboard LED on D13
static const pin_t _LED_TEST   = { &DDRB, &PORTB, &PINB, PB5 };

// <<< pin definitions >>>
const pin_t PIN_RTEST = { &DDRD, &PORTD, &PIND, PD2 };
const pin_t PIN_CTEST = { &DDRD, &PORTD, &PIND, PD3 };
const pin_t PIN_DISCH = { &DDRD, &PORTD, &PIND, PD4 };
const pin_t PIN_COMP = { &DDRB, &PORTB, &PINB, PB4 };

// TODO CHECK
const pin_t PIN_555_OUT = { &DDRD, &PORTD, &PIND, PD5 };

const pin_t LED_R_LOW  = { &DDRD, &PORTD, &PIND, PD6 };
const pin_t LED_R_HIGH = { &DDRD, &PORTD, &PIND, PD7 };
const pin_t LED_OPEN   = { &DDRB, &PORTB, &PINB, PB0 };
const pin_t LED_C_LOW  = { &DDRB, &PORTB, &PINB, PB1 };
const pin_t LED_C_HIGH = { &DDRB, &PORTB, &PINB, PB2 };
const pin_t LED_DISCH  = { &DDRB, &PORTB, &PINB, PB3 };

const pin_t LED_TEST = { &DDRB, &PORTB, &PINB, PB5 };

// <<< pin mode >>>
// set DDR bit high
void pin_mode_output(pin_t p) {
    *p.ddr |= (1 << p.bit);
}

// set DDR bit low, and optionally set PORT bit for pullup
void pin_mode_input(pin_t p) {
    *p.ddr &= ~(1 << p.bit);
    *p.port &= ~(1 << p.bit); // disable pullup
}

// set DDR bit low, and optionally set PORT bit for pullup
// NOTE: not sure how necessary this function even is.. will keep for now
void pin_mode_input_pullup(pin_t p) {
    *p.ddr  &= ~(1 << p.bit);
    *p.port |=  (1 << p.bit); // enable pullup
}

// <<< digital read / write >>>
// write high if value is true, low if value is false
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
// init function, set reference voltage and prescaler
void adc_init(void) {
    ADMUX  = (1 << REFS0);
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

// perform read on channel 0, return 10 bit result
uint16_t adc_read(uint8_t channel) {
    ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADC;
}

uint16_t analog_read(void) {
    return adc_read(0); // A0 = channel 0
}

// <<< timer / millis >>>
volatile uint32_t ms_count = 0;

ISR(TIMER1_COMPA_vect) {
    ms_count++;
}

void timer1_init(void) {
    // CTC mode: timer resets to 0 when it matches OCR1A
    // WGM12 = 1 enables CTC mode (bit 3 of TCCR1B)
    // CS11 + CS10 = prescaler /64
    // at 16MHz: 16000000 / 64 / 250 = 1000 Hz = 1ms per tick
    // at 8MHz:  8000000  / 64 / 125 = 1000 Hz = 1ms per tick
    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);

    // compare match value
    // at 16MHz: OCR1A = 249 (counts 0..249 = 250 ticks)
    // at 8MHz:  OCR1A = 124 (counts 0..124 = 125 ticks)
    OCR1A  = 249;

    // enable compare match interrupt
    TIMSK1 = (1 << OCIE1A);

    // reset counter
    TCNT1  = 0;
}

uint32_t millis(void) {
    uint32_t val;
    cli(); // disable interrupts to read ms_count atomically
    val = ms_count;
    sei(); // enable interrupts again
    return val;
}

// thin wrapper over internal _delay_ms function,
// which is a busy loop that blocks the CPU for the specified time
// we don't just do _delay_ms(ms) directly because doing so gives
// error: __builtin_avr_delay_cycles expects a compile time integer constant
// doing it this way means we always call it with a constant
void delay(unsigned int ms) {
    while (ms--) {
        _delay_ms(1);
    }
}

uint32_t measure_frequency(void) {
    // configure Timer1 as external pulse counter on T1 (PD5)
    // CS12|CS11|CS10 = external clock on T1, rising edge
    TCCR1A = 0;
    TCCR1B = (1 << CS12) | (1 << CS11) | (1 << CS10);
    TCNT1 = 0;

    // count pulses for 100ms
    // using _delay_ms since we don't want timer1 for millis here
    uint8_t i;
    for (i = 0; i < 100; i++) {
        _delay_ms(1);
    }

    uint16_t pulses = TCNT1;

    // restore Timer1 to CTC mode for millis
    timer1_init();

    // frequency in Hz = pulses / 0.1s = pulses * 10
    return (uint32_t)pulses * 10;
}


void single_led_flash(pin_t led, unsigned int duration_ms) {
    digital_write(led, HIGH);
    delay(duration_ms);
    digital_write(led, LOW);
}

uint16_t abs_diff(int16_t x) {
    return x < 0 ? -x : x;
}