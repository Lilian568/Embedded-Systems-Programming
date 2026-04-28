//
// Created by lily on 11/6/2024.
//
#include "rotary_encoder.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"
#include <inttypes.h> //PRIu64
#include <stdio.h>

#define QUEUE_SIZE 50

static RotaryEncoder rotary_encoder;

void rot_a_isr()
{
    if (gpio_get(rotary_encoder.pin_b) == 0)
    {
        RotaryEncoderEvent event = ROT_CW;
        queue_try_add(&(rotary_encoder.events), &event);
    }
    else
    {
        RotaryEncoderEvent event = ROT_CCW;
        queue_try_add(&(rotary_encoder.events), &event);
    }
}

void rot_sw_isr()
{
    if (time_us_64() - rotary_encoder.last_time_press >
        rotary_encoder.debounce_delay * 1000)
    {
        RotaryEncoderEvent event = ROT_PRESSED;
        queue_try_add(&(rotary_encoder.events), &event);
        rotary_encoder.last_time_press = time_us_64();
    }
}

void rot_isr(uint gpio, uint32_t events)
{
    if (rotary_encoder.rotate_enabled && gpio == rotary_encoder.pin_a)
    {
        rot_a_isr();
    }
    else if (gpio == rotary_encoder.pin_sw)
    {
        rot_sw_isr();
    }
}

void init_rotary_encoder(uint pin_a, uint pin_b, uint pin_sw,
                         uint64_t debounce_delay)
{
    // Initialize the rotary encoder
    rotary_encoder.pin_a = pin_a;
    rotary_encoder.pin_b = pin_b;
    rotary_encoder.pin_sw = pin_sw;
    rotary_encoder.debounce_delay = debounce_delay;
    rotary_encoder.rotate_enabled = true;

    gpio_init(pin_a);
    gpio_set_dir(pin_a, GPIO_IN);

    gpio_init(pin_b);
    gpio_set_dir(pin_b, GPIO_IN);

    gpio_init(pin_sw);
    gpio_set_dir(pin_sw, GPIO_IN);
    gpio_pull_up(pin_sw);

    queue_init(&(rotary_encoder.events), sizeof(RotaryEncoderEvent),
               QUEUE_SIZE);

    // Set interrupt handler
    gpio_set_irq_enabled_with_callback(pin_a, GPIO_IRQ_EDGE_RISE, true,
                                       &rot_isr);
    gpio_set_irq_enabled(pin_sw, GPIO_IRQ_EDGE_RISE, true);
}

void set_rot_enable(bool enable)
{
    rotary_encoder.rotate_enabled = enable;
}

bool get_rotary_encoder_event(RotaryEncoderEvent *event)
{
    return queue_try_remove(&(rotary_encoder.events), event);
}