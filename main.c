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
// Vx charges through PIN_CTEST resistor, ADC polls until threshold
// t = R * C, at 5 tau cap is 99.3% charged (~4.65V) --> ADC = 950
// using 465kΩ charge resistor at 16MHz:
// 1nF  --> 5 * 465000 * 0.000000001 = 2.3ms
// 3nF  --> 5 * 465000 * 0.000000003 = 6.98ms
// 10nF --> 5 * 465000 * 0.000000010 = 23.25ms
#define ADC_CHARGED 950
#define T_C_SPLIT_MS 7
#define T_TIMEOUT_MS 50

// <<< stable reading constants >>>
// take 5 readings 5ms apart, confirm within 10 ADC counts
#define STABLE_READINGS 5
#define STABLE_TOLERANCE 10

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
    // discharge again then charge through 465kΩ, time to 5 tau
    discharge();

    // configure cap test, start charging up Vx
    pin_mode_output(PIN_CTEST);
    digital_write(PIN_CTEST, HIGH);
    uint32_t t_start = millis();
    uint32_t t_elapsed = 0;

    // poll ADC until Vx reaches 5 tau (~4.65V) or timeout
    while (analog_read() < ADC_CHARGED) {
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
}

// <<< entry point >>>
int main(void) {
    setup();
    while (true) {
        loop();
    }
}