#include "hal.h"

// <<< ADC thresholds >>>
#define ADC_SHORT_MAX     3
#define ADC_R_SPLIT       385
#define ADC_NOT_RESISTOR  900
#define STABLE_READINGS   5
#define STABLE_TOLERANCE  10

// <<< diode thresholds >>>
// regular diode (0.7V drop): fwd ≈ 4.3V → ADC ≈ 879
// red LED (1.8V drop):       fwd ≈ 1.8V → ADC ≈ 369
// white LED (2.6V drop):     fwd ≈ 2.4V → ADC ≈ 491
// blue LED (3.3V drop):      fwd ≈ 1.7V → ADC ≈ 348
// reverse biased (any):      rev ≈ 0V   → ADC < 150
#define ADC_DIODE_FWD_MIN 100
#define ADC_DIODE_FWD_MAX 950
#define ADC_DIODE_REV_MAX 150

// <<< capacitor timing >>>
// V(t) = 5 * (1 - e^(-t/RC)), solve for V(t) = 1.1V
// t = 0.248 * RC
// R = 1MΩ, /64 prescaler = 4us/tick at 16MHz
//
// 1nF  -> 0.248ms -> 62 ticks
// 3nF  -> 0.744ms -> 186 ticks
// 10nF -> 2.48ms  -> 620 ticks
#define TICKS_OPEN_MAX  20
#define TICKS_SPLIT     186
#define TICKS_TIMEOUT   1000
#define CAP_TIMEOUT     0xFFFF

// <<< discharge >>>
static void discharge(void) {
    digital_write(PIN_DISCH, HIGH);
    delay(300);
    digital_write(PIN_DISCH, LOW);
    delay(50);
}

// <<< diode test >>>
static bool test_diode(void) {
    uint16_t fwd, rev;

    // forward: D2 HIGH, D12 actively LOW, transistor on
    pin_mode_output(PIN_RTEST);
    pin_mode_output(PIN_DTEST);
    digital_write(PIN_GNDCTL, HIGH);
    digital_write(PIN_RTEST, HIGH);
    digital_write(PIN_DTEST, LOW);
    delay(10);
    fwd = analog_read();

    // reverse: D12 HIGH, D2 actively LOW, transistor off
    digital_write(PIN_GNDCTL, LOW);
    digital_write(PIN_RTEST, LOW);
    digital_write(PIN_DTEST, HIGH);
    delay(10);
    rev = analog_read();

    // restore
    digital_write(PIN_RTEST, LOW);
    digital_write(PIN_DTEST, LOW);
    digital_write(PIN_GNDCTL, HIGH);

    pin_mode_input(PIN_RTEST);
    pin_mode_input(PIN_DTEST);

    // orientation 1: anode on D2 side
    bool fwd_orientation = (fwd > ADC_DIODE_FWD_MIN && fwd < ADC_DIODE_FWD_MAX && rev < ADC_DIODE_REV_MAX);

    // orientation 2: anode on D12 side
    bool rev_orientation = (fwd > 900 && rev > ADC_DIODE_FWD_MIN && rev < ADC_DIODE_FWD_MAX);

    if (fwd_orientation || rev_orientation) {
        single_led_flash(LED_DIODE, 1000);
        return true;
    }

    return false;
}

// <<< resistor test >>>
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

// <<< capacitor test >>>
static uint16_t measure_cap_ticks(void) {
    discharge();

    TCNT1 = 0;
    TIFR1 = (1 << ICF1);

    pin_mode_output(PIN_CTEST);
    digital_write(PIN_CTEST, HIGH);

    while (!(TIFR1 & (1 << ICF1))) {
        if (TCNT1 > TICKS_TIMEOUT) {
            digital_write(PIN_CTEST, LOW);
            pin_mode_input(PIN_CTEST);
            return CAP_TIMEOUT;
        }
    }

    uint16_t ticks = ICR1;
    digital_write(PIN_CTEST, LOW);
    pin_mode_input(PIN_CTEST);
    return ticks;
}

static void test_cap_or_open(void) {
    uint16_t ticks = measure_cap_ticks();

    if (ticks == CAP_TIMEOUT || ticks < TICKS_OPEN_MAX) {
        single_led_flash(LED_OPEN, 1000);
    } else if (ticks < TICKS_SPLIT) {
        single_led_flash(LED_C_LOW, 1000);
    } else {
        single_led_flash(LED_C_HIGH, 1000);
    }
}

int main(void) {
    adc_init();
    timer1_init();
    analog_comp_init();

    pin_mode_output(PIN_RTEST);
    pin_mode_output(PIN_CTEST);
    pin_mode_output(PIN_DISCH);
    pin_mode_output(PIN_DTEST);
    pin_mode_output(PIN_GNDCTL);
    pin_mode_output(LED_SHORT);
    pin_mode_output(LED_R_LOW);
    pin_mode_output(LED_R_HIGH);
    pin_mode_output(LED_C_LOW);
    pin_mode_output(LED_C_HIGH);
    pin_mode_output(LED_OPEN);
    pin_mode_output(LED_DISCH);
    pin_mode_output(LED_DIODE);

    // AIN1 (D7) must be input with no pullup for comparator
    DDRD  &= ~(1 << PD7);
    PORTD &= ~(1 << PD7);

    digital_write(PIN_RTEST, LOW);
    digital_write(PIN_CTEST, LOW);
    digital_write(PIN_DISCH, LOW);
    digital_write(PIN_DTEST, LOW);
    digital_write(PIN_GNDCTL, HIGH);

    // startup sequence
    single_led_flash(LED_R_LOW,  300);
    single_led_flash(LED_R_HIGH, 300);
    single_led_flash(LED_SHORT,  300);

    single_led_flash(LED_C_LOW,  300);
    single_led_flash(LED_C_HIGH, 300);
    single_led_flash(LED_OPEN,   300);

    single_led_flash(LED_DISCH,  300);
    single_led_flash(LED_DIODE,  300);

    while (1) {
        if (test_diode()) {
            delay(500);
            continue;
        }

        if (test_resistor_or_short()) {
            delay(500);
            continue;
        }

        test_cap_or_open();
        delay(500);
    }
}