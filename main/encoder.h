#pragma once

#include <stdbool.h>
#include "driver/gpio.h"

/* KY-040 дає 4 квадратурні кроки на один "клік" (перевірено self-test'ом). */
#define ENCODER_STEPS_PER_DETENT    4

/* Налаштовує піни, вмикає переривання на CLK/DT, підтяжку на SW. */
void encoder_init(gpio_num_t clk, gpio_num_t dt, gpio_num_t sw);

/* Кількість "кліків" з моменту попереднього виклику.
 * > 0 - за годинниковою (CW), < 0 - проти (CCW). */
int encoder_take_detents(void);

/* Відкидає все, що накрутилось (наприклад, поки грала мелодія). */
void encoder_discard(void);

/* Вимикає переривання енкодера (блокування до перезавантаження). */
void encoder_disable(void);

/* true рівно один раз у момент натискання кнопки (з антибрязкотом).
 * Викликати періодично з головного циклу. */
bool encoder_button_pressed(void);
