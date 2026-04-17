#include "hal.h"
#include <avr/interrupt.h>
#include <util/delay.h>

// <<< pin definitions >>>

// temporary test pin - onboard LED on D13
static const pin_t _LED_TEST   = { &DDRB, &PORTB, &PINB, PB5 };

// <<< pin definitions >>>
const pin_t PIN_RTEST  = { &DDRD, &PORTD, &PIND, PD3 };
const pin_t PIN_DISCH  = { &DDRD, &PORTD, &PIND, PD4 };
const pin_t PIN_CTEST  = { &DDRD, &PORTD, &PIND, PD5 };
const pin_t LED_R_LOW  = { &DDRB, &PORTB, &PINB, PB6 };
const pin_t LED_R_HIGH = { &DDRB, &PORTB, &PINB, PB7 };
const pin_t LED_OPEN   = { &DDRB, &PORTB, &PINB, PB0 };
const pin_t LED_C_LOW  = { &DDRB, &PORTB, &PINB, PB1 };
const pin_t LED_C_HIGH = { &DDRB, &PORTB, &PINB, PB2 };
const pin_t LED_DISCH  = { &DDRB, &PORTB, &PINB, PB3 };
const pin_t LED_TEST   = { &DDRB, &PORTB, &PINB, PB5 };

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

// <<< high level helper functions >>>
void leds_off(void) {
    digital_write(LED_R_LOW, LOW);
    digital_write(LED_R_HIGH, LOW);
    digital_write(LED_OPEN, LOW);
    digital_write(LED_C_LOW, LOW);
    digital_write(LED_C_HIGH, LOW);
}

void single_led_flash(pin_t led_pin) {
    digital_write(led_pin, HIGH);
    delay(1000);
    digital_write(led_pin, LOW);
}

void discharge(void) {
    // turn off R_TEST pin
    digital_write(PIN_RTEST, LOW);
    pin_mode_input(PIN_RTEST);
    digital_write(PIN_CTEST, LOW);

    // turn off C_TEST pin
    pin_mode_input(PIN_CTEST);

    // turn on discharge pin and light LED
    digital_write(LED_DISCH, HIGH);
    digital_write(PIN_DISCH, HIGH);
    delay(1000);

    // turn off above
    digital_write(PIN_DISCH, LOW);
    digital_write(LED_DISCH, LOW);
    delay(500);
}

// green LED, open circuit
void show_open(void) {
    digital_write(LED_OPEN, HIGH);
    delay(500);
    digital_write(LED_OPEN, LOW);
}

uint16_t abs_diff(int16_t x) {
    return x < 0 ? -x : x;
}