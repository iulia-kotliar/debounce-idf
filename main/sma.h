#pragma once
#include <stdint.h>

#define SMA_N 16

typedef struct {
    uint16_t buf[SMA_N];
    uint32_t sum;      // біжуча сума всіх значень у буфері
    uint8_t  idx;      // куди писати наступне значення
    uint8_t  count;    // скільки значень вже є (для «розгону»)
} sma_t;

void     sma_init(sma_t *s);
uint16_t sma_update(sma_t *s, uint16_t sample);