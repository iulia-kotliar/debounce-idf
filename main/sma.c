#include "sma.h"
#include <string.h>

void sma_init(sma_t *s) {
    memset(s, 0, sizeof(*s));
}

uint16_t sma_update(sma_t *s, uint16_t sample) {
    s->sum -= s->buf[s->idx];      
    s->buf[s->idx] = sample;      
    s->sum += sample;

    s->idx = (s->idx + 1) % SMA_N; 
    if (s->count < SMA_N) s->count++;

    return s->sum / s->count;      
}