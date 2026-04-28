#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"

// NEW
// include the queue library
#include "pico/util/queue.h"

#define MIN_BRIGHTNESS 0
#define DEFAULT_BRIGHTNESS 505
#define MAX_BRIGHTNESS 1000
#define STEP 10

enum { rot_a_pin = 10, rot_b_pin = 11, rot_sw_pin = 12 }; // Rotary switch and encoder pins
enum leds { led1 = 20, led2 = 21, led3 = 22 };
volatile bool led_on = true;
int brightness = MAX_BRIGHTNESS;
int last_brightness = DEFAULT_BRIGHTNESS; // NEW: To store last brightness level before turning off
bool was_button_pressed = true;
int last_encoder_a = 0;
int last_encoder_b = 0;

// NEW
// define a queue
queue_t event_queue;

void encoder_irq_handler(uint gpio, uint32_t events) {
    int encoder_a = gpio_get(rot_a_pin);
    int encoder_b = gpio_get(rot_b_pin);
    if (encoder_a != last_encoder_a) {
        if (encoder_a != encoder_b) {

            // NEW
            // add the brightness change to the queue
            int brightness_change = STEP;
            queue_try_add(&event_queue, &brightness_change);

        } else {

            // NEW
            // add the brightness change to the queue
            int brightness_change = -STEP;
            queue_try_add(&event_queue, &brightness_change);

        }
    }
    last_encoder_a = encoder_a;
    last_encoder_b = encoder_b;
}

int main() {
    stdio_init_all();
    gpio_init(led1);
    gpio_set_function(led1, GPIO_FUNC_PWM);
    gpio_init(led2);
    gpio_set_function(led2, GPIO_FUNC_PWM);
    gpio_init(led3);
    gpio_set_function(led3, GPIO_FUNC_PWM);

    uint slice_num1 = pwm_gpio_to_slice_num(led1);
    uint channel1 = pwm_gpio_to_channel(led1);
    uint slice_num2 = pwm_gpio_to_slice_num(led2);
    uint channel2 = pwm_gpio_to_channel(led2);
    uint slice_num3 = pwm_gpio_to_slice_num(led3);
    uint channel3 = pwm_gpio_to_channel(led3);

    pwm_config config = pwm_get_default_config();
    pwm_config_set_wrap(&config, 1000);

    pwm_init(slice_num1, &config, false);
    pwm_init(slice_num2, &config, false);
    pwm_init(slice_num3, &config, false);

    pwm_set_chan_level(slice_num1, channel1, DEFAULT_BRIGHTNESS);
    pwm_set_chan_level(slice_num2, channel2, DEFAULT_BRIGHTNESS);
    pwm_set_chan_level(slice_num3, channel3, DEFAULT_BRIGHTNESS);

    pwm_set_enabled(slice_num1, true);
    pwm_set_enabled(slice_num2, true);
    pwm_set_enabled(slice_num3, true);

    gpio_init(rot_a_pin);
    gpio_set_dir(rot_a_pin, GPIO_IN);

    gpio_init(rot_b_pin);
    gpio_set_dir(rot_b_pin, GPIO_IN);

    gpio_init(rot_sw_pin);
    gpio_set_dir(rot_sw_pin, GPIO_IN);
    gpio_pull_up(rot_sw_pin);

    gpio_set_irq_enabled_with_callback(rot_a_pin, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &encoder_irq_handler);

    // NEW
    // define a variable to store the brightness change
    int brightness_change;
    // initialize the queue
    queue_init(&event_queue, sizeof(int), 10);

    while (true) {
        bool sw_pressed = gpio_get(rot_sw_pin);
        if (!sw_pressed && was_button_pressed) {
            if (led_on) {
                // Check if brightness is 0 and set to 50% brightness
                if (brightness == MIN_BRIGHTNESS) {
                    brightness = DEFAULT_BRIGHTNESS;   // Set to 50% brightness
                    int was_zero_brightness = false;      // Reset the zero brightness flag
                    pwm_set_gpio_level(led1, brightness);
                    pwm_set_gpio_level(led2, brightness);
                    pwm_set_gpio_level(led3, brightness);
                } else {
                    // Turn LEDs off and save brightness level if above 0
                    if (brightness > MIN_BRIGHTNESS) {
                        last_brightness = brightness;
                    }
                    led_on = false;
                    pwm_set_gpio_level(led1, MIN_BRIGHTNESS);
                    pwm_set_gpio_level(led2, MIN_BRIGHTNESS);
                    pwm_set_gpio_level(led3, MIN_BRIGHTNESS);
                }
            } else {
                // Turn LEDs on, restoring last brightness or setting to 50% if 0
                led_on = true;
                brightness = (last_brightness > MIN_BRIGHTNESS) ? last_brightness : DEFAULT_BRIGHTNESS;
                pwm_set_gpio_level(led1, brightness);
                pwm_set_gpio_level(led2, brightness);
                pwm_set_gpio_level(led3, brightness);
            }
            sleep_ms(STEP);
        }

        was_button_pressed = sw_pressed;

        // NEW
        // read and process brightness changes from the queue
        while (queue_try_remove(&event_queue, &brightness_change)) {
            printf("Brightness change: %d\n", brightness_change);
            brightness += brightness_change;
            if (brightness > MAX_BRIGHTNESS) {
                brightness = MAX_BRIGHTNESS;
            } else if (brightness < MIN_BRIGHTNESS) {
                brightness = MIN_BRIGHTNESS;
            }
            if (led_on) {
                pwm_set_gpio_level(led1, brightness);
                pwm_set_gpio_level(led2, brightness);
                pwm_set_gpio_level(led3, brightness);
            }
        }

        sleep_ms(STEP);
    }
}

