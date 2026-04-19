#include "hal.h"
#include <avr/interrupt.h>

// note that all relevant pin definitions are in hal.h

// <<< ADC thresholds >>>
// ADC is 10-bit: 0-1023, 1 count = 4.88mV at 5V reference
// open circuit: Vx floats high --> ADC > 950
// resistor test uses 5kΩ R_TEST voltage divider:
//   1kΩ --> Vx = 0.83V --> ADC = 170
//   3kΩ --> Vx = 1.88V --> ADC = 385
//   5kΩ --> Vx = 2.50V --> ADC = 512
//   10kΩ --> Vx = 3.33V --> ADC = 682
// not-resistor threshold: ADC > 800
// range split at 3kΩ boundary: ADC = 385
#define ADC_OPEN 950
#define ADC_NOT_RESISTOR 800
#define ADC_R_SPLIT 385

// <<< capacitor timing >>>
// comparator fires when Vx crosses VREF = 2.5V (0.69 tau)
// t = 0.69 * R * C, using 465kΩ charge resistor at 16MHz:
// 1nF  --> 0.69 * 465000 * 0.000000001 = 0.32ms
// 3nF  --> 0.69 * 465000 * 0.000000003 = 0.96ms
// 10nF --> 0.69 * 465000 * 0.000000010 = 3.21ms
#define T_C_SPLIT_MS 1
#define T_TIMEOUT_MS 10

// TODO CHECK
// // <<< 555 timer capacitor test >>>
// // 555 in astable mode: f = 1.44 / ((Ra + 2*Rb) * C)
// // Ra = 1kΩ, Rb = 10kΩ --> Ra + 2*Rb = 21000
// // 1nF  --> f = 1.44 / (21000 * 1e-9)  = 68571 Hz
// // 3nF  --> f = 1.44 / (21000 * 3e-9)  = 22857 Hz
// // 10nF --> f = 1.44 / (21000 * 10e-9) = 6857  Hz
// #define TIMER_RA 1000
// #define TIMER_RB 10000
// #define F_3NF_HZ  22857  // split boundary at 3nF
// #define F_OPEN_HZ 6857   // below this --> above 10nF or open

// <<< stable reading constants >>>
// take 5 readings 5ms apart, confirm within 10 ADC counts
#define STABLE_READINGS 5
#define STABLE_TOLERANCE 10

// <<< comparator interrupt flag >>>
// set by PCINT0 ISR when comparator output goes HIGH
volatile bool comp_triggered = false;

ISR(PCINT0_vect) {
    // fires on any change on port B pins
    // check PIN_COMP (PB4 = D12) went HIGH
    if (digital_read(PIN_COMP)) {
        comp_triggered = true;
    }
}

// <<< helper functions >>>
// turn all output LEDs off
void leds_off(void) {
    digital_write(LED_R_LOW, LOW);
    digital_write(LED_R_HIGH, LOW);
    digital_write(LED_OPEN, LOW);
    digital_write(LED_C_LOW, LOW);
    digital_write(LED_C_HIGH, LOW);
}

// discharge Vx to 0V via BC548 transistor
void discharge(void) {
    digital_write(PIN_RTEST, LOW);
    pin_mode_input(PIN_RTEST);
    digital_write(PIN_CTEST, LOW);
    pin_mode_input(PIN_CTEST);

    digital_write(LED_DISCH, HIGH);
    digital_write(PIN_DISCH, HIGH);
    delay(500);

    digital_write(PIN_DISCH, LOW);
    digital_write(LED_DISCH, LOW);
    delay(50);
}

// helper: flash green open circuit LED briefly
void show_open(void) {
    single_led_flash(LED_OPEN, 1000);
}

// setup
void setup(void) {
    // setup peripherals
    timer1_init();
    adc_init();

    // enable pin change interrupt on port B for PIN_COMP (PB4)
    PCICR  |= (1 << PCIE0);
    PCMSK0 |= (1 << PCINT4);

    sei(); // enable interrupts

    // configure all pins to initial state
    pin_mode_output(PIN_RTEST);
    pin_mode_output(PIN_DISCH);
    pin_mode_output(PIN_CTEST);
    pin_mode_output(LED_R_LOW);
    pin_mode_output(LED_R_HIGH);
    pin_mode_output(LED_OPEN);
    pin_mode_output(LED_C_LOW);
    pin_mode_output(LED_C_HIGH);
    pin_mode_output(LED_DISCH);
    pin_mode_input(PIN_COMP);

    // set all pins LOW
    digital_write(PIN_RTEST, LOW);
    digital_write(PIN_DISCH, LOW);
    digital_write(PIN_CTEST, LOW);
    digital_write(LED_DISCH, LOW);
    leds_off();

    // startup sequence - flash each LED in order
    single_led_flash(LED_R_LOW, 1000);
    single_led_flash(LED_R_HIGH, 1000);
    single_led_flash(LED_OPEN, 1000);
    single_led_flash(LED_C_LOW, 1000);
    single_led_flash(LED_C_HIGH, 1000);
    single_led_flash(LED_DISCH, 1000);
}

// <<< main loop >>>
void loop(void) {
    leds_off();

    // always discharge first so Vx starts from 0V
    discharge();

    // drive Vx through 5kΩ
    pin_mode_output(PIN_RTEST);
    digital_write(PIN_RTEST, HIGH);
    delay(10);

    // <<< test 0: open circuit >>>
    // open circuit has no path to GND so Vx floats to 5V
    uint16_t idle_read = analog_read();
    if (idle_read > ADC_OPEN) {
        digital_write(PIN_RTEST, LOW);
        pin_mode_input(PIN_RTEST);
        show_open();
        delay(1000);
        return;
    }

    // <<< test 1: resistor >>>
    // take 5 readings spaced 5ms apart, check stability
    int16_t readings[STABLE_READINGS];
    for (uint8_t i = 0; i < STABLE_READINGS; i++) {
        readings[i] = analog_read();
        delay(5);
    }

    // a reading is only stable if multiple readings are
    // within tolerance
    bool stable = true;
    for (uint8_t i = 1; i < STABLE_READINGS; i++) {
        if (abs_diff(readings[i] - readings[i-1]) > STABLE_TOLERANCE) {
            stable = false;
            break;
        }
    }

    // take our main reading to be the last one
    int16_t adc_val = readings[STABLE_READINGS - 1];
    digital_write(PIN_RTEST, LOW);
    pin_mode_input(PIN_RTEST);

    // resistor result
    if (stable && adc_val < ADC_NOT_RESISTOR) {
        if (adc_val < ADC_R_SPLIT) {
            single_led_flash(LED_R_LOW, 1000);
        } else {
            single_led_flash(LED_R_HIGH, 1000);
        }
        delay(1000);
        return;
    }

    // <<< test 2: capacitor >>>
    // discharge again, then charge through 465kΩ
    // comparator interrupt fires when Vx crosses VREF (2.5V = 0.69 tau)
    discharge();

    comp_triggered = false;
    pin_mode_output(PIN_CTEST);
    digital_write(PIN_CTEST, HIGH);
    uint32_t t_start = millis();
    uint32_t t_elapsed = 0;

    // wait for comparator interrupt or timeout
    while (!comp_triggered) {
        t_elapsed = millis() - t_start;
        if (t_elapsed >= T_TIMEOUT_MS) break;
    }

    t_elapsed = millis() - t_start;
    digital_write(PIN_CTEST, LOW);
    pin_mode_input(PIN_CTEST);

    // cap result
    if (t_elapsed < T_C_SPLIT_MS) {
        single_led_flash(LED_C_LOW, 1000);
    } else {
        single_led_flash(LED_C_HIGH, 1000);
    }

    delay(1000);

    // TODO CHECK
    // // <<< test 2: capacitor via 555 timer >>>
    // // 555 in astable mode oscillates at f = 1.44 / ((Ra + 2*Rb) * C)
    // // higher frequency = smaller capacitance
    // // open circuit = 555 won't oscillate, freq near 0
    // discharge();

    // // T1 pin must be input so 555 can drive it
    // pin_mode_input(PIN_555_OUT);

    // // measure frequency from 555 output
    // uint32_t freq = measure_frequency();

    // // open circuit or no cap -- 555 won't oscillate
    // if (freq < F_OPEN_HZ) {
    //     show_open();
    //     delay(1000);
    //     return;
    // }

    // // classify by frequency -- higher freq = smaller cap
    // if (freq > F_3NF_HZ) {
    //     single_led_flash(LED_C_LOW, 1000);
    // } else {
    //     single_led_flash(LED_C_HIGH, 1000);
    // }

    // delay(1000);
}

// <<< entry point >>>
int main(void) {
    setup();
    while (true) {
        loop();
    }
}