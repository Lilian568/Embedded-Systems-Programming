//
// Created by lily on 11/6/2024.
//
#include "leds.h"
#include "hardware/pwm.h"

#define LED_PWM_CLK_DIV 125.0f
#define LED_PWM_WRAP 999
#define LED_PWM_LEVEL_MAX (LED_PWM_WRAP + 1)

void init_led(Led *led, uint gpio_pin)
{
    led->slice = pwm_gpio_to_slice_num(gpio_pin);
    led->channel = pwm_gpio_to_channel(gpio_pin);
    pwm_set_enabled(led->slice, false);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, LED_PWM_CLK_DIV);
    pwm_config_set_wrap(&config, LED_PWM_WRAP);
    pwm_init(led->slice, &config, false);
    pwm_set_chan_level(led->slice, led->channel, 0);
    gpio_set_function(gpio_pin, GPIO_FUNC_PWM);
    pwm_set_enabled(led->slice, true);
}

void set_leds_brightness(Led *leds, uint num_leds, float brightness)
{
    for (int i = 0; i < num_leds; i++)
    {
        int level = (int)(brightness / 100 * LED_PWM_LEVEL_MAX);
        pwm_set_chan_level(leds[i].slice, leds[i].channel, level);
    }
}