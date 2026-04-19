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
#define ADC_R_SPLIT        385
#define ADC_NOT_RESISTOR   800

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

// returns:
// - captured tick count if comparator threshold was crossed
// - CAP_TIMEOUT if no crossing occurred before timeout
// static uint16_t measure_cap_ticks(void) {
//     discharge();

//     TCNT1 = 0;
//     TIFR1 = (1 << ICF1); // clear any pending input capture flag

//     pin_mode_output(PIN_CTEST);
//     digital_write(PIN_CTEST, HIGH);

//     while (!(TIFR1 & (1 << ICF1))) {
//         if (TCNT1 > TICKS_TIMEOUT) {
//             digital_write(PIN_CTEST, LOW);
//             pin_mode_input(PIN_CTEST);
//             return CAP_TIMEOUT;
//         }
//     }

//     {
//         uint16_t ticks = ICR1;
//         digital_write(PIN_CTEST, LOW);
//         pin_mode_input(PIN_CTEST);
//         return ticks;
//     }
// }

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

            single_led_flash(LED_TEST, 200); // timeout marker
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

// setup
static void setup(void) {
    timer1_init();
    adc_init();
    analog_comp_init();

    pin_mode_output(PIN_RTEST);
    pin_mode_output(PIN_CTEST);
    pin_mode_output(PIN_DISCH);

    pin_mode_output(LED_R_LOW);
    pin_mode_output(LED_R_HIGH);
    pin_mode_output(LED_OPEN);
    pin_mode_output(LED_C_LOW);
    pin_mode_output(LED_C_HIGH);
    pin_mode_output(LED_DISCH);

    // D6 is output, D7 is input
    DDRD  &= ~(1 << PD7);
    PORTD &= ~(1 << PD7);

    digital_write(PIN_RTEST, LOW);
    digital_write(PIN_CTEST, LOW);
    digital_write(PIN_DISCH, LOW);
    digital_write(LED_DISCH, LOW);
    leds_off();

    // startup LED test
    single_led_flash(LED_R_LOW,  300);
    single_led_flash(LED_R_HIGH, 300);
    single_led_flash(LED_OPEN,   300);
    single_led_flash(LED_C_LOW,  300);
    single_led_flash(LED_C_HIGH, 300);
    single_led_flash(LED_DISCH,  300);
}

// <<< main loop >>>
static void loop(void) {
    int16_t readings[STABLE_READINGS];
    bool stable = true;
    int16_t adc_val;
    uint8_t i;

    leds_off();

    // ------------------------------------------------------------------
    // test 1: resistor test
    // ------------------------------------------------------------------
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

    adc_val = readings[STABLE_READINGS - 1];

    digital_write(PIN_RTEST, LOW);
    pin_mode_input(PIN_RTEST);

    if (stable && adc_val < ADC_NOT_RESISTOR) {
        if (adc_val < ADC_R_SPLIT) {
            single_led_flash(LED_R_LOW, 1000);
        } else {
            single_led_flash(LED_R_HIGH, 1000);
        }
        delay(500);
        return;
    }

    // ------------------------------------------------------------------
    // test 2: capacitor vs open circuit
    // ------------------------------------------------------------------
    {
        uint16_t ticks = measure_cap_ticks();

        if (ticks == CAP_TIMEOUT) {
            show_open();
        } else if (ticks < TICKS_SPLIT) {
            single_led_flash(LED_C_LOW, 1000);
        } else {
            single_led_flash(LED_C_HIGH, 1000);
        }
    }

    delay(500);
}

// <<< entry point >>>
int main(void) {
    setup();

    while (true) {
        loop();
    }
}