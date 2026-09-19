#pragma once

#include <stdint.h>
#include <stdbool.h>

#define SAFE_MAX_DIGITS     8

typedef enum {
    SAFE_ST_ENTERING,      
    SAFE_ST_OPEN,           
    SAFE_ST_LOCKED_OUT,   
} safe_state_t;

typedef enum {
    SAFE_EV_NONE,           
    SAFE_EV_DIGIT_STARTED, 
    SAFE_EV_DIGIT_CHANGED,  
    SAFE_EV_CODE_OK,        
    SAFE_EV_CODE_WRONG,     
    SAFE_EV_RESET,         
    SAFE_EV_LOCKOUT,       
    SAFE_EV_CLOSED,         
} safe_event_t;

typedef struct {
    /* конфігурація */
    uint8_t code[SAFE_MAX_DIGITS];
    uint8_t code_len;
    uint8_t max_attempts;

    /* стан */
    safe_state_t state;
    uint8_t entered[SAFE_MAX_DIGITS];
    uint8_t entered_count;     
    uint8_t current;          
    bool    has_current;
    int8_t  dir;               
    uint8_t attempts_used;
} safe_t;

bool safe_init(safe_t *s, const uint8_t *code, uint8_t code_len, uint8_t max_attempts);

safe_event_t safe_on_tick(safe_t *s, int dir);

safe_event_t safe_on_button(safe_t *s);

static inline uint8_t safe_attempts_left(const safe_t *s)
{
    return (uint8_t)(s->max_attempts - s->attempts_used);
}

static inline uint8_t safe_attempt_number(const safe_t *s)
{
    return (uint8_t)(s->attempts_used + 1);
}
