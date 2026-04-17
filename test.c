#include "hal.h"

int main(void) {
    pin_mode_output(LED_TEST);

    while (1) {
        digital_write(LED_TEST, HIGH);
        delay(1000);
        digital_write(LED_TEST, LOW);
        delay(1000);
    }
}