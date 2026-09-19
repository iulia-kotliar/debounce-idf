#pragma once
/*
 * Логіка сейфа: скінченний автомат без залежності від заліза.
 * Отримує події (тік енкодера, кнопка) і повертає, що сталось.
 * Що з цим робити (звук, серво, лог) вирішує main.c.
 *
 * Правила введення:
 *   - перший тік (у будь-який бік) починає цифру зі значенням 0;
 *   - кожен наступний тік у тому ж напрямку: +1 (9 -> 0, циклічно);
 *   - тік у протилежному напрямку підтверджує цифру і починає наступну з 0;
 *   - коли підтверджено останню цифру, код одразу перевіряється;
 *   - кнопка: скидання введення, рахується як спроба.
 */

#include <stdint.h>
#include <stdbool.h>

#define SAFE_MAX_DIGITS     8

typedef enum {
    SAFE_ST_ENTERING,       /* йде введення коду */
    SAFE_ST_OPEN,           /* код правильний, замок відкрито */
    SAFE_ST_LOCKED_OUT,     /* спроби вичерпано, блокування до перезавантаження */
} safe_state_t;

typedef enum {
    SAFE_EV_NONE,           /* подію проігноровано (напр., тік у стані OPEN) */
    SAFE_EV_DIGIT_STARTED,  /* почалась нова цифра (значення 0) */
    SAFE_EV_DIGIT_CHANGED,  /* поточна цифра змінилась */
    SAFE_EV_CODE_OK,        /* код правильний -> OPEN */
    SAFE_EV_CODE_WRONG,     /* код неправильний, є ще спроби */
    SAFE_EV_RESET,          /* кнопка: скидання, є ще спроби */
    SAFE_EV_LOCKOUT,        /* остання спроба (WRONG або RESET) -> LOCKED_OUT */
    SAFE_EV_CLOSED,         /* кнопка у стані OPEN: замок закрито, нове введення */
} safe_event_t;

typedef struct {
    /* конфігурація */
    uint8_t code[SAFE_MAX_DIGITS];
    uint8_t code_len;
    uint8_t max_attempts;

    /* стан */
    safe_state_t state;
    uint8_t entered[SAFE_MAX_DIGITS];
    uint8_t entered_count;      /* підтверджені цифри */
    uint8_t current;            /* поточна (ще не підтверджена) цифра */
    bool    has_current;
    int8_t  dir;                /* напрямок поточної цифри: +1 / -1, 0 - ще немає */
    uint8_t attempts_used;
} safe_t;

/* false, якщо параметри некоректні (довжина 1..8, цифри 0..9, спроб >= 1) */
bool safe_init(safe_t *s, const uint8_t *code, uint8_t code_len, uint8_t max_attempts);

/* dir: +1 (CW) або -1 (CCW) */
safe_event_t safe_on_tick(safe_t *s, int dir);

safe_event_t safe_on_button(safe_t *s);

static inline uint8_t safe_attempts_left(const safe_t *s)
{
    return (uint8_t)(s->max_attempts - s->attempts_used);
}

/* Номер поточної спроби, починаючи з 1 */
static inline uint8_t safe_attempt_number(const safe_t *s)
{
    return (uint8_t)(s->attempts_used + 1);
}
