#include "hal.h"

// <<< ADC thresholds >>> 
// ADC is 10-bit: 0-1023, 1 count = 4.88mV at 5V reference
//
// resistor test uses 5kΩ R_TEST voltage divider:
//   1kΩ   -> Vx = 0.83V -> ADC = 170
//   3kΩ   -> Vx = 1.88V -> ADC = 385
//   5kΩ   -> Vx = 2.50V -> ADC = 512
//   10kΩ  -> Vx = 3.33V -> ADC = 682
//   30kΩ  -> Vx = 4.29V -> ADC = 877
//   100kΩ -> Vx = 4.76V -> ADC = 974
//
// if it is a resistor, the ADC reading should settle to a stable DC value
// if it is a capacitor or open circuit, it should not look like a valid stable resistor divider
#define ADC_SHORT_MAX      3
#define ADC_R_SPLIT        385
#define ADC_NOT_RESISTOR   1010

// <<< diode thresholds >>>
// with 5kΩ on both sides of the DUT, a forward-biased diode usually gives
// one midscale reading and one rail reading, depending on orientation
#define ADC_DIODE_DIFF_THRESHOLD  200
#define ADC_DIODE_MID_MIN         250
#define ADC_DIODE_MID_MAX         800
#define ADC_DIODE_RAIL_LOW        100
#define ADC_DIODE_RAIL_HIGH       900

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

// <<< stable reading constants >>> 
#define STABLE_READINGS   5
#define STABLE_TOLERANCE  10

// <<< helper functions >>>
static void leds_off(void) {
    digital_write(LED_R_LOW, LOW);
    digital_write(LED_R_HIGH, LOW);
    digital_write(LED_OPEN, LOW);
    digital_write(LED_C_LOW, LOW);
    digital_write(LED_C_HIGH, LOW);
    digital_write(LED_DISCH, LOW);
    digital_write(LED_DIODE, LOW);
    digital_write(LED_SHORT, LOW);
    digital_write(LED_TEST, LOW);
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
            // single_led_flash(LED_TEST, 200); // timeout marker
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
    uint16_t a_b, b_a;

    // start neutral
    digital_write(PIN_RTEST, LOW);
    pin_mode_input(PIN_RTEST);
    digital_write(PIN_DTEST, LOW);
    pin_mode_input(PIN_DTEST);

    delay(5);

    pin_mode_output(PIN_RTEST);
    pin_mode_output(PIN_DTEST);

    // polarity 1: D2 high, D12 low
    single_led_flash(LED_R_LOW, 1000); // sanity check that we are actually driving D2 high

    digital_write(PIN_RTEST, HIGH);
    digital_write(PIN_DTEST, LOW);
    delay(10);
    a_b = analog_read();

    // polarity 2: D2 low, D12 high
    single_led_flash(LED_R_HIGH, 1000); // sanity check that we are actually driving D12 high

    digital_write(PIN_RTEST, LOW);
    digital_write(PIN_DTEST, HIGH);
    delay(10);
    b_a = analog_read();

    // release pins
    digital_write(PIN_RTEST, LOW);
    pin_mode_input(PIN_RTEST);
    digital_write(PIN_DTEST, LOW);
    pin_mode_input(PIN_DTEST);

    uint16_t diff = (a_b > b_a) ? (a_b - b_a) : (b_a - a_b);

    bool a_mid  = (a_b > ADC_DIODE_MID_MIN)  && (a_b < ADC_DIODE_MID_MAX);
    bool b_mid  = (b_a > ADC_DIODE_MID_MIN)  && (b_a < ADC_DIODE_MID_MAX);
    bool a_rail = (a_b < ADC_DIODE_RAIL_LOW) || (a_b > ADC_DIODE_RAIL_HIGH);
    bool b_rail = (b_a < ADC_DIODE_RAIL_LOW) || (b_a > ADC_DIODE_RAIL_HIGH);

    if (diff > ADC_DIODE_DIFF_THRESHOLD && ((a_mid && b_rail) || (b_mid && a_rail))) {
        single_led_flash(LED_DIODE, 1000);
        return true;
    }

    return false;
}

// static bool test_diode(void) {

//     uint16_t a_b, b_a;

//     pin_mode_output(PIN_RTEST);
//     pin_mode_output(PIN_DTEST);

//     digital_write(PIN_RTEST, LOW);
//     digital_write(PIN_DTEST, LOW);

//     digital_write(PIN_RTEST, HIGH);
//     digital_write(PIN_DTEST, LOW);
//     delay(10);
//     a_b = analog_read();

//     digital_write(PIN_RTEST, LOW);
//     digital_write(PIN_DTEST, LOW);

//     digital_write(PIN_RTEST, LOW);
//     digital_write(PIN_DTEST, HIGH);
//     delay(10);
//     b_a = analog_read();

//     digital_write(PIN_RTEST, LOW);
//     pin_mode_input(PIN_RTEST);
//     digital_write(PIN_DTEST, LOW);
//     pin_mode_input(PIN_DTEST);

//     if (a_b > 1000 && b_a > 1000) {
//         return false;
//     }

//     uint16_t diff = (a_b > b_a) ? (a_b -b_a) : (b_a - a_b);

//     if (diff > ADC_DIODE_DIFF_THRESHOLD) {
//         single_led_flash(LED_DIODE, 1000);
//         return true;
//     }

//     return false;
// }

static void test_cap_or_open(void) {
    single_led_flash(LED_TEST, 1000);

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

    // test drive pins
    pin_mode_output(PIN_RTEST);
    pin_mode_output(PIN_CTEST);
    pin_mode_output(PIN_DISCH);

    // D12 should default to high-Z so it doesn't interfere
    digital_write(PIN_DTEST, LOW);
    pin_mode_input(PIN_DTEST);

    // LEDs
    pin_mode_output(LED_R_LOW);
    pin_mode_output(LED_R_HIGH);
    pin_mode_output(LED_OPEN);
    pin_mode_output(LED_C_LOW);
    pin_mode_output(LED_C_HIGH);
    pin_mode_output(LED_DISCH);
    pin_mode_output(LED_DIODE);
    pin_mode_output(LED_SHORT);
    pin_mode_output(LED_TEST);

    // comparator input pin must remain input, no pullup
    DDRD  &= ~(1 << PD7);
    PORTD &= ~(1 << PD7);

    // initialise drive pins low
    digital_write(PIN_RTEST, LOW);
    digital_write(PIN_CTEST, LOW);
    digital_write(PIN_DISCH, LOW);

    // keep inactive test pins floating by default
    pin_mode_input(PIN_RTEST);
    pin_mode_input(PIN_CTEST);

    // all LEDs off before startup sequence
    leds_off();

    // startup LED test
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
