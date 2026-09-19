#pragma once

#include <stdint.h>
#include <stddef.h>
#include "driver/gpio.h"

/* Нота: частота 0 = пауза */
typedef struct {
    uint16_t freq_hz;
    uint16_t duration_ms;
} buzzer_note_t;

/* Пасивний бузер на LEDC_TIMER_1 / LEDC_CHANNEL_1 (таймер 0 зайнятий серво). */
void buzzer_init(gpio_num_t pin);

/* Блокуючий тон. freq_hz = 0 - пауза. */
void buzzer_tone(uint32_t freq_hz, uint32_t duration_ms);

/* Блокуюче програє послідовність нот. */
void buzzer_play(const buzzer_note_t *notes, size_t count);

#define BUZZER_PLAY(melody)  buzzer_play((melody), sizeof(melody) / sizeof((melody)[0]))
