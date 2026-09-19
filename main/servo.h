#pragma once

#include "driver/gpio.h"

#define SERVO_MIN_DEG   10      
#define SERVO_MAX_DEG   170

void servo_init(gpio_num_t pin);

void servo_set_angle(int deg);
