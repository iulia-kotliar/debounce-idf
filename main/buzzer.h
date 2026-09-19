#pragma once

#include <stdint.h>
#include <stddef.h>
#include "driver/gpio.h"

typedef struct {
    uint16_t freq_hz;
    uint16_t duration_ms;
} buzzer_note_t;

void buzzer_init(gpio_num_t pin);

void buzzer_tone(uint32_t freq_hz, uint32_t duration_ms);

void buzzer_play(const buzzer_note_t *notes, size_t count);

#define BUZZER_PLAY(melody)  buzzer_play((melody), sizeof(melody) / sizeof((melody)[0]))
