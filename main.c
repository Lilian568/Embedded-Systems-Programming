#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"

#define TOP 1000
#define DIVIDER 125
#define STEP 50

enum buttons {b1 = 7, b2, b3};
enum leds {led1 = 20, led2 = 21, led3 = 22};
bool button_pressed(uint btn);

void setup_pwm(uint led_pin, uint16_t level) {
    gpio_set_function(led_pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(led_pin);
    uint channel = pwm_gpio_to_channel(led_pin);
    pwm_set_clkdiv(slice_num, DIVIDER);
    pwm_set_wrap(slice_num, TOP);
    pwm_set_chan_level(slice_num, channel, level);
    pwm_set_enabled(slice_num, true);
}

int main() {
    const uint bt_pin1 = b1;
    const uint bt_pin2 = b2;
    const uint bt_pin3 = b3;
    const uint led_pins[] = {led1, led2, led3};
    const int led_count = sizeof(led_pins) / sizeof(led_pins[0]);

    uint16_t level = TOP;
    bool led_on = false;

    for (int i = 0; i < led_count; i++) {
        gpio_init(led_pins[i]);
        gpio_set_dir(led_pins[i], GPIO_OUT);
        setup_pwm(led_pins[i], 0); // Start with LEDs off
    }

    gpio_init(bt_pin1);
    gpio_set_dir(bt_pin1, GPIO_IN);
    gpio_pull_up(bt_pin1);

    gpio_init(bt_pin2);
    gpio_set_dir(bt_pin2, GPIO_IN);
    gpio_pull_up(bt_pin2);

    gpio_init(bt_pin3);
    gpio_set_dir(bt_pin3, GPIO_IN);
    gpio_pull_up(bt_pin3);

    stdio_init_all();

    while (true) {
        if (button_pressed(bt_pin2)) {
            led_on = !led_on;
            level = (level == 0) ? 500 : level; // Default brightness if currently at 0
            for (int i = 0; i < led_count; i++) {
                uint slice_num = pwm_gpio_to_slice_num(led_pins[i]);
                uint channel = pwm_gpio_to_channel(led_pins[i]);
                pwm_set_chan_level(slice_num, channel, led_on ? level : 0);
            }
            while (button_pressed(bt_pin2));
        }

        if (led_on) {
            if (!gpio_get(bt_pin1)) { // Decrease brightness
                level = (level >= STEP) ? level - STEP : 0;
                if (level == 0) led_on = false; // Turn off LEDs if brightness reaches 0
                for (int i = 0; i < led_count; i++) {
                    uint slice_num = pwm_gpio_to_slice_num(led_pins[i]);
                    uint channel = pwm_gpio_to_channel(led_pins[i]);
                    pwm_set_chan_level(slice_num, channel, level);
                }
            }
            if (!gpio_get(bt_pin3)) { // Increase brightness
                level = (level <= TOP - STEP) ? level + STEP : TOP;
                for (int i = 0; i < led_count; i++) {
                    uint slice_num = pwm_gpio_to_slice_num(led_pins[i]);
                    uint channel = pwm_gpio_to_channel(led_pins[i]);
                    pwm_set_chan_level(slice_num, channel, level);
                }
                led_on = true; // Ensure LEDs stay on when brightness is increased from 0
            }
        } else {
            if (!gpio_get(bt_pin3)) { // Turn LEDs back on, increasing brightness from 0
                led_on = true;
                level = STEP; // Start increasing from the first step
                for (int i = 0; i < led_count; i++) {
                    uint slice_num = pwm_gpio_to_slice_num(led_pins[i]);
                    uint channel = pwm_gpio_to_channel(led_pins[i]);
                    pwm_set_chan_level(slice_num, channel, level);
                }
            }

            if (!gpio_get(bt_pin2)) { // Turn LEDs on at 50% brightness
                led_on = true;
                level = TOP / 2;
                for (int i = 0; i < led_count; i++) {
                    uint slice_num = pwm_gpio_to_slice_num(led_pins[i]);
                    uint channel = pwm_gpio_to_channel(led_pins[i]);
                    pwm_set_chan_level(slice_num, channel, level);
                }
            }
        }
    }
}

bool button_pressed(uint btn) {
    int press = 0;
    int release = 0;
    while (press < 3 && release < 3) {
        if(!gpio_get(btn)) {
            press++;
            release = 0;
        } else {
            release++;
            press = 0;
        }
        sleep_ms(40);
    }
    return (press > release);
}
