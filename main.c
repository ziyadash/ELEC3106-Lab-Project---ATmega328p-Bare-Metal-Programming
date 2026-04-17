#include "hal.h"
#include <avr/interrupt.h>
#include <stdbool.h>

static void setup(void) {
    timer1_init();
    adc_init();
    sei();

    pin_mode_output(PIN_RTEST);
    pin_mode_output(PIN_DISCH);
    pin_mode_output(PIN_CTEST);
    pin_mode_output(LED_R_LOW);
    pin_mode_output(LED_R_HIGH);
    pin_mode_output(LED_OPEN);
    pin_mode_output(LED_C_LOW);
    pin_mode_output(LED_C_HIGH);
    pin_mode_output(LED_DISCH);

    leds_off();
}

static void loop(void) {
    // real application logic goes here
}

int main(void) {
    // setup();
    // while (true) {
    //     loop();
    // }
    pin_mode_output(LED_TEST);

    while (1) {
        digital_write(LED_TEST, HIGH);
        delay(1000);
        digital_write(LED_TEST, LOW);
        delay(1000);
    }
}