#ifndef DIMMER_LAB1__LEDS_H
#define DIMMER_LAB1__LEDS_H
#include "pico/stdlib.h"

typedef struct
{
    uint slice;
    uint channel;
} Led;

void init_led(Led *led, uint gpio_pin);
void set_leds_brightness(Led *leds, uint num_leds, float brightness);

#endif // DIMMER_LAB1__LEDS_H
