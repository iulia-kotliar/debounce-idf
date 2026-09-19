#pragma once

#include <stdbool.h>
#include "driver/gpio.h"

#define ENCODER_STEPS_PER_DETENT    4

void encoder_init(gpio_num_t clk, gpio_num_t dt, gpio_num_t sw);

int encoder_take_detents(void);

void encoder_discard(void);

void encoder_disable(void);

bool encoder_button_pressed(void);
