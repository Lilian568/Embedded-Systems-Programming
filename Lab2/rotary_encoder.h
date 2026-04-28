#ifndef DIMMER_LAB1__BUTTON_H
#define DIMMER_LAB1__BUTTON_H

#include "pico/stdlib.h"
#include "pico/util/queue.h"

typedef struct
{
    uint pin_a;
    uint pin_b;
    uint pin_sw;
    uint64_t debounce_delay;
    queue_t events;
    uint64_t last_time_press;
    bool rotate_enabled;
} RotaryEncoder;

typedef enum
{
    ROT_CW,
    ROT_CCW,
    ROT_PRESSED,
} RotaryEncoderEvent;

void rot_a_isr();

void rot_sw_isr();

void rot_isr(uint gpio, uint32_t events);

void set_rot_enable(bool enable);

void init_rotary_encoder(uint pin_a, uint pin_b, uint pin_sw,
                         uint64_t debounce_delay);

bool get_rotary_encoder_event(RotaryEncoderEvent *event);

#endif // DIMMER_LAB1__BUTTON_H
