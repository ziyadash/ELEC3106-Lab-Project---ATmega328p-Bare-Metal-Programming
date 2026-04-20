#include "hal.h"

// <<< ADC thresholds >>> 
// ADC is 10-bit: 0-1023, 1 count = 4.88mV at 5V reference
//
// resistor test uses 5kΩ R_TEST voltage divider:
//   1kΩ  -> Vx = 0.83V -> ADC = 170
//   3kΩ  -> Vx = 1.88V -> ADC = 385
//   5kΩ  -> Vx = 2.50V -> ADC = 512
//   10kΩ -> Vx = 3.33V -> ADC = 682
//
// if it is a resistor, the ADC reading should settle to a stable DC value
// if it is a capacitor or open circuit, it should not look like a valid stable resistor divider
#define ADC_SHORT_MAX      30
#define ADC_R_SPLIT        385
#define ADC_NOT_RESISTOR   800

// <<< diode thresholds >>>
// one polarity should conduct and pull Vx low
// the other should block and leave Vx high
#define ADC_DIODE_DIFF_THRESHOLD  600
#define ADC_DIODE_LOW_MAX         250
#define ADC_DIODE_HIGH_MIN        800

// <<< capacitor timing >>> 
// comparator threshold is internal 1.1V bandgap
// V(t) = 5 * (1 - e^(-t/RC))
// solve for V(t) = 1.1V:
//
// 1.1 = 5 * (1 - e^(-t/RC))
// e^(-t/RC) = 0.78
// t = -RC ln(0.78) = 0.248RC
//
// R = 1MΩ:
//
// 1nF  -> 0.248 ms ->  62 ticks at 4us/tick
// 3nF  -> 0.744 ms -> 186 ticks
// 10nF -> 2.48  ms -> 620 ticks
#define TICKS_OPEN_MAX 20
#define TICKS_SPLIT    186
#define TICKS_TIMEOUT  1000
#define CAP_TIMEOUT    0xFFFF

// <<< diode thresholds >>>
// forward biased diode: Vx = 5V - 0.6V = 4.4V --> ADC = 902
// reverse biased diode: Vx floats near 5V --> ADC > 950
// resistor: both directions give same ADC reading
// asymmetry threshold: if |forward - reverse| > this, it's a diode
#define ADC_DIODE_FORWARD_MIN  800  // forward drop pulls Vx below this
#define ADC_DIODE_ASYMMETRY    200  // difference between forward and reverse
#define ADC_DIODE_DIFF_THRESHOLD 600

// <<< stable reading constants >>> 
#define STABLE_READINGS   5
#define STABLE_TOLERANCE  10

// <<< helper functions >>>
static void leds_off(void) {
    digital_write(LED_R_LOW,  LOW);
    digital_write(LED_R_HIGH, LOW);
    digital_write(LED_OPEN,   LOW);
    digital_write(LED_C_LOW,  LOW);
    digital_write(LED_C_HIGH, LOW);
    digital_write(LED_DISCH,  LOW);
    digital_write(LED_DIODE,  LOW);
    digital_write(LED_SHORT,  LOW);
    digital_write(LED_TEST,   LOW);
}

static void show_open(void) {
    single_led_flash(LED_OPEN, 1000);
}

static void discharge(void) {
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

    {
        uint16_t ticks = ICR1;
        digital_write(PIN_CTEST, LOW);
        pin_mode_input(PIN_CTEST);
        single_led_flash(LED_TEST, 50); // capture marker
        return ticks;
    }
}

// <<< tests >>>
// returns true if handled, false to fall through to next test

static bool test_resistor_or_short(void) {
    int16_t readings[STABLE_READINGS];
    bool stable = true;
    uint8_t i;

    discharge();

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

static bool test_diode(void) {
    uint16_t fwd, rev;

    // forward: D2 HIGH, D12 actively LOW, transistor on
    pin_mode_output(PIN_RTEST);
    pin_mode_output(PIN_DTEST);

    // polarity 1: D2 high, D12 low
    digital_write(PIN_RTEST, HIGH);
    digital_write(PIN_DTEST, LOW);
    delay(10);
    a_b = analog_read();

    // polarity 2: D2 low, D12 high
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

    bool case1 = (a_b < ADC_DIODE_LOW_MAX) && (b_a > ADC_DIODE_HIGH_MIN);
    bool case2 = (b_a < ADC_DIODE_LOW_MAX) && (a_b > ADC_DIODE_HIGH_MIN);

    if (diff > ADC_DIODE_DIFF_THRESHOLD && (case1 || case2)) {
        single_led_flash(LED_DIODE, 1000);
        return true;
    }

    return false;
}

static void test_cap_or_open(void) {
    uint16_t ticks = measure_cap_ticks();

    if (ticks == CAP_TIMEOUT || ticks < TICKS_OPEN_MAX) {
        show_open();
    } else if (ticks < TICKS_SPLIT) {
        single_led_flash(LED_C_LOW, 1000);
    } else {
        single_led_flash(LED_C_HIGH, 1000);
    }
}

// setup
static void setup(void) {
    timer1_init();
    adc_init();
    analog_comp_init();

    pin_mode_output(PIN_RTEST);
    pin_mode_output(PIN_CTEST);
    pin_mode_output(PIN_DISCH);
    pin_mode_output(PIN_GNDCTL);

    // PIN_DTEST defaults to input so it doesn't interfere with other tests
    digital_write(PIN_DTEST, LOW);
    pin_mode_input(PIN_DTEST);

    pin_mode_output(LED_R_LOW);
    pin_mode_output(LED_R_HIGH);
    pin_mode_output(LED_OPEN);
    pin_mode_output(LED_C_LOW);
    pin_mode_output(LED_C_HIGH);
    pin_mode_output(LED_DISCH);
    pin_mode_output(LED_DIODE);
    pin_mode_output(LED_SHORT);
    pin_mode_output(LED_TEST);

    // AIN1 (D7) must be input with no pullup for comparator
    DDRD  &= ~(1 << PD7);
    PORTD &= ~(1 << PD7);

    digital_write(PIN_RTEST, LOW);
    digital_write(PIN_CTEST, LOW);
    digital_write(PIN_DISCH, LOW);

    // PIN_GNDCTL HIGH by default so Vb is always connected to GND
    // except during diode reverse test
    digital_write(PIN_GNDCTL, HIGH);

    pin_mode_input(PIN_RTEST);
    pin_mode_input(PIN_CTEST);

    leds_off();

    single_led_flash(LED_R_LOW,  300);
    single_led_flash(LED_R_HIGH, 300);
    single_led_flash(LED_OPEN,   300);
    single_led_flash(LED_C_LOW,  300);
    single_led_flash(LED_C_HIGH, 300);
    single_led_flash(LED_DISCH,  300);
    single_led_flash(LED_DIODE,  300);
    single_led_flash(LED_SHORT,  300);
    single_led_flash(LED_TEST,   300);

    leds_off();
}

// <<< main loop >>>
static void loop(void) {
    leds_off();

    if (test_diode()) {
        delay(500);
        return;
    }

    if (test_resistor_or_short()) {
        delay(500);
        return;
    }

    test_cap_or_open();
    delay(500);
}

// <<< entry point >>>
int main(void) {
    setup();

    while (true) {
        loop();
    }
}