#include "hal.h"

// diode detection based on asymmetry
// forward biased: Vx somewhere between 0.5V and 4.5V (any diode/LED conducts)
// reverse biased: Vx near 0V (transistor on, Vb=GND, diode blocks)
//
// regular diode (0.7V drop): fwd ≈ 4.3V → ADC ≈ 879
// red LED (1.8V drop):       fwd ≈ 1.8V → ADC ≈ 369
// white LED (2.6V drop):     fwd ≈ 2.4V → ADC ≈ 491
// blue LED (3.3V drop):      fwd ≈ 1.7V → ADC ≈ 348
//
// reverse biased (any): rev ≈ 0V → ADC < 150
#define ADC_DIODE_FWD_MIN 100
#define ADC_DIODE_FWD_MAX 950
#define ADC_DIODE_REV_MAX 150

static bool test_diode(void) {
    uint16_t fwd, rev;

    // forward: D2 HIGH, D12 actively LOW, transistor on
    pin_mode_output(PIN_RTEST);
    pin_mode_output(PIN_DTEST);
    digital_write(PIN_GNDCTL, HIGH);
    digital_write(PIN_RTEST, HIGH);
    digital_write(PIN_DTEST, LOW);
    delay(1000);
    fwd = analog_read();

    // reverse: D12 HIGH, D2 actively LOW, transistor off
    digital_write(PIN_GNDCTL, LOW);
    digital_write(PIN_RTEST, LOW);
    digital_write(PIN_DTEST, HIGH);
    delay(1000);
    rev = analog_read();

    // restore
    digital_write(PIN_RTEST, LOW);
    digital_write(PIN_DTEST, LOW);
    digital_write(PIN_GNDCTL, HIGH);

    // orientation 1: anode on D2 side
    // fwd mid-scale (conducting), rev near 0 (blocking)
    bool fwd_orientation = (fwd > ADC_DIODE_FWD_MIN && fwd < ADC_DIODE_FWD_MAX && rev < ADC_DIODE_REV_MAX);

    // orientation 2: anode on D12 side
    // fwd near rail (blocking from D2 side), rev mid-scale (conducting)
    bool rev_orientation = (fwd > 900 && rev > ADC_DIODE_FWD_MIN && rev < ADC_DIODE_FWD_MAX);

    bool something_connected = (fwd > 50 && fwd < 1000) || (rev > 50 && rev < 1000);

    if (something_connected && (fwd_orientation || rev_orientation)) {
        single_led_flash(LED_DIODE, 1000);
        return true;
    }

    return false;
}

int main(void) {
    adc_init();

    pin_mode_output(PIN_RTEST);
    pin_mode_output(PIN_DTEST);
    pin_mode_output(PIN_GNDCTL);
    pin_mode_output(LED_DIODE);
    pin_mode_output(LED_TEST);

    digital_write(PIN_RTEST, LOW);
    digital_write(PIN_DTEST, LOW);
    digital_write(PIN_GNDCTL, HIGH);
    digital_write(LED_DIODE, LOW);
    digital_write(LED_TEST, LOW);

    single_led_flash(LED_DIODE, 1000);

    while (1) {
        if (test_diode()) {
            delay(500);
        } else {
            digital_write(LED_DIODE, LOW);
            delay(200);
        }
    }
}