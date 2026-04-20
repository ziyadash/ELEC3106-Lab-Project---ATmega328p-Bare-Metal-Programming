#include "hal.h"

// ADC thresholds for diode detection based on LTspice simulation
// orientation 1: anode on D2 side
//   fwd ≈ 3.3V → ADC ≈ 676, rev ≈ 0V → ADC ≈ 0
// orientation 2: anode on D12 side
//   fwd ≈ 5V → ADC ≈ 1023, rev ≈ 1.6V → ADC ≈ 328
#define ADC_SHORT_MAX     3
#define ADC_R_SPLIT       385
#define ADC_NOT_RESISTOR  1010

#define STABLE_READINGS   5
#define STABLE_TOLERANCE  10

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

    // orientation 1: anode on D2 side, transistor on shorts Vb to GND
    // only top 5kΩ in circuit, Vx = 5V - LED_drop ≈ 1.8V → ADC ≈ 369
    bool fwd_orientation = (fwd > 250 && fwd < 500 && rev < 200);

    // orientation 2: anode on D12 side
    // fwd near rail ~1023, rev mid-low ~328
    bool rev_orientation = (fwd > 900 && rev > 200 && rev < 500);

    bool something_connected = (fwd > 50 && fwd < 1000) || (rev > 50 && rev < 1000);

    if (something_connected && (fwd_orientation || rev_orientation)) {
        single_led_flash(LED_DIODE, 1000);
        return true;
    }

    return false;
}

static bool test_resistor_or_short(void) {
    int16_t readings[STABLE_READINGS];
    bool stable = true;
    uint8_t i;

    pin_mode_output(PIN_RTEST);
    digital_write(PIN_RTEST, HIGH);
    delay(10);

    for (i = 0; i < STABLE_READINGS; i++) {
        readings[i] = (int16_t)analog_read();
        delay(5);
    }

    for (i = 1; i < STABLE_READINGS; i++) {
        if (abs_diff(readings[i] - readings[i - 1]) > STABLE_TOLERANCE) {
            stable = false;
            break;
        }
    }

    int16_t adc_val = readings[STABLE_READINGS - 1];

    digital_write(PIN_RTEST, LOW);
    pin_mode_input(PIN_RTEST);

    if (!stable || adc_val >= ADC_NOT_RESISTOR) {
        return false;
    }

    if (adc_val < ADC_SHORT_MAX) {
        single_led_flash(LED_SHORT, 1000);
    } else if (adc_val < ADC_R_SPLIT) {
        single_led_flash(LED_R_LOW, 1000);
    } else {
        single_led_flash(LED_R_HIGH, 1000);
    }

    return true;
}

int main(void) {
    adc_init();

    pin_mode_output(PIN_RTEST);
    pin_mode_output(PIN_DTEST);
    pin_mode_output(PIN_GNDCTL);
    pin_mode_output(LED_DIODE);
    pin_mode_output(LED_TEST);
    pin_mode_output(LED_R_LOW);
    pin_mode_output(LED_R_HIGH);
    pin_mode_output(LED_SHORT);

    digital_write(PIN_RTEST, LOW);
    digital_write(PIN_DTEST, LOW);
    digital_write(PIN_GNDCTL, HIGH);
    digital_write(LED_DIODE, LOW);
    digital_write(LED_TEST, LOW);
    digital_write(LED_R_LOW, LOW);
    digital_write(LED_R_HIGH, LOW);
    digital_write(LED_SHORT, LOW);

    single_led_flash(LED_DIODE,  300);
    single_led_flash(LED_R_LOW,  300);
    single_led_flash(LED_R_HIGH, 300);
    single_led_flash(LED_SHORT,  300);

    while (1) {
        if (test_diode()) {
            delay(500);
            continue;
        }

        // if (test_resistor_or_short()) {
        //     delay(500);
        //     continue;
        // }

        digital_write(LED_DIODE,  LOW);
        digital_write(LED_R_LOW,  LOW);
        digital_write(LED_R_HIGH, LOW);
        digital_write(LED_SHORT,  LOW);
        delay(200);
    }
}