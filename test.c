#include "hal.h"

int main(void) {
    // configure relevant pins
    pin_mode_output(PIN_RTEST);
    pin_mode_output(PIN_CTEST);
    pin_mode_output(PIN_DISCH);
    pin_mode_output(LED_DISCH);

    // start with everything off
    digital_write(PIN_RTEST, LOW);
    digital_write(PIN_CTEST, LOW);
    digital_write(PIN_DISCH, LOW);
    digital_write(LED_DISCH, LOW);

    while (1) {
        // -----------------------------
        // DISCHARGE PHASE
        // -----------------------------
        // make sure both charge paths are disconnected
        digital_write(PIN_RTEST, LOW);
        pin_mode_input(PIN_RTEST);

        digital_write(PIN_CTEST, LOW);
        pin_mode_input(PIN_CTEST);

        // turn on discharge transistor and discharge LED
        digital_write(PIN_DISCH, HIGH);
        digital_write(LED_DISCH, HIGH);

        // hold discharge for 30 seconds
        delay(30000);

        // turn discharge off
        digital_write(PIN_DISCH, LOW);
        digital_write(LED_DISCH, LOW);

        // small gap between phases
        digital_write(LED_TEST, HIGH);
        delay(500);
        digital_write(LED_TEST, LOW);

        // -----------------------------
        // CHARGE PHASE
        // -----------------------------
        // ensure discharge transistor is off
        digital_write(PIN_DISCH, LOW);
        digital_write(LED_DISCH, LOW);

        // charge through the capacitor test resistor
        pin_mode_output(PIN_CTEST);
        digital_write(PIN_CTEST, HIGH);

        // hold charge for 30 seconds
        delay(30000);

        // turn charge off and float the pin again
        digital_write(PIN_CTEST, LOW);
        pin_mode_input(PIN_CTEST);

        // small gap before next cycle
        digital_write(LED_TEST, HIGH);
        delay(500);
        digital_write(LED_TEST, LOW);
    }
}

// #include "hal.h"

// int main(void) {
//     // configure test LED pin as output
//     pin_mode_output(LED_TEST);

//     while (1) {
//         digital_write(LED_TEST, HIGH);
//         delay(1000);

//         digital_write(LED_TEST, LOW);
//         delay(1000);
//     }
// }