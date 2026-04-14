#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

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
    volatile uint8_t *ddr; // data direction register
    volatile uint8_t *port; // output / pullup register
    volatile uint8_t *pin; // input register
    uint8_t bit; // which bit within the register
} pin_t;

// <<< pin definitions >>>
static const pin_t PIN_RTEST = { &DDRD, &PORTD, &PIND, PD3 };
static const pin_t PIN_DISCH = { &DDRD, &PORTD, &PIND, PD4 };
static const pin_t PIN_CTEST = { &DDRD, &PORTD, &PIND, PD5 };
static const pin_t LED_R_LOW = { &DDRB, &PORTB, &PINB, PB6 };
static const pin_t LED_R_HIGH = { &DDRB, &PORTB, &PINB, PB7 };
static const pin_t LED_OPEN = { &DDRB, &PORTB, &PINB, PB0 };
static const pin_t LED_C_LOW = { &DDRB, &PORTB, &PINB, PB1 };
static const pin_t LED_C_HIGH = { &DDRB, &PORTB, &PINB, PB2 };
static const pin_t LED_DISCH = { &DDRB, &PORTB, &PINB, PB3 };

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
    delay(1000 / 16);
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
    delay(1000 / 16);

    // turn off above
    digital_write(PIN_DISCH, LOW);
    digital_write(LED_DISCH, LOW);
    delay(500 / 16);
}

// green LED, open circuit
void show_open(void) {
    digital_write(LED_OPEN, HIGH);
    delay(500 / 16);
    digital_write(LED_OPEN, LOW);
}

// <<< setup / loop / main >>>
void setup(void) {
    // configure internal state
    timer1_init();
    adc_init();
    sei(); // enable interrupts

    // configure pins
    pin_mode_output(PIN_RTEST);
    pin_mode_output(PIN_DISCH);
    pin_mode_output(PIN_CTEST);
    pin_mode_output(LED_R_LOW);
    pin_mode_output(LED_R_HIGH);
    pin_mode_output(LED_OPEN);
    pin_mode_output(LED_C_LOW);
    pin_mode_output(LED_C_HIGH);
    pin_mode_output(LED_DISCH);

    // light up LEDs one by one
    leds_off();

    // flash on and off each LED in order
    single_led_flash(LED_R_LOW);
    single_led_flash(LED_R_HIGH);
    single_led_flash(LED_OPEN);
    single_led_flash(LED_C_LOW);
    single_led_flash(LED_C_HIGH);
    single_led_flash(LED_DISCH);
}

void loop(void) {
    // todo copy the arduino code and translate it
}

int main(void) {
    setup();
    while (true) {
        loop();
    }
}