#pragma once

#include "driver/gpio.h"

#define SERVO_MIN_DEG   10      /* безпечні межі, щоб не впиратись у механічний упор */
#define SERVO_MAX_DEG   170

/* SG90 на LEDC_TIMER_0 / LEDC_CHANNEL_0, 50 Гц. */
void servo_init(gpio_num_t pin);

/* Кут обмежується до SERVO_MIN_DEG..SERVO_MAX_DEG. */
void servo_set_angle(int deg);
