#include "safe.h"

#include <string.h>

static void clear_input(safe_t *s)
{
    memset(s->entered, 0, sizeof(s->entered));
    s->entered_count = 0;
    s->current = 0;
    s->has_current = false;
    s->dir = 0;
}

bool safe_init(safe_t *s, const uint8_t *code, uint8_t code_len, uint8_t max_attempts)
{
    if (s == NULL || code == NULL) return false;
    if (code_len == 0 || code_len > SAFE_MAX_DIGITS) return false;
    if (max_attempts == 0) return false;
    for (uint8_t i = 0; i < code_len; i++) {
        if (code[i] > 9) return false;
    }

    memset(s, 0, sizeof(*s));
    memcpy(s->code, code, code_len);
    s->code_len = code_len;
    s->max_attempts = max_attempts;
    s->state = SAFE_ST_ENTERING;
    clear_input(s);
    return true;
}

/* Спроба використана (невірний код або RESET). */
static safe_event_t consume_attempt(safe_t *s, safe_event_t ev_if_left)
{
    s->attempts_used++;
    clear_input(s);

    if (s->attempts_used >= s->max_attempts) {
        s->state = SAFE_ST_LOCKED_OUT;
        return SAFE_EV_LOCKOUT;
    }
    return ev_if_left;
}

static safe_event_t check_code(safe_t *s)
{
    if (memcmp(s->entered, s->code, s->code_len) == 0) {
        s->state = SAFE_ST_OPEN;
        s->attempts_used = 0;
        clear_input(s);
        return SAFE_EV_CODE_OK;
    }
    return consume_attempt(s, SAFE_EV_CODE_WRONG);
}

safe_event_t safe_on_tick(safe_t *s, int dir)
{
    if (s->state != SAFE_ST_ENTERING || dir == 0) {
        return SAFE_EV_NONE;
    }
    dir = (dir > 0) ? +1 : -1;

    /* Той самий напрямок: інкремент поточної цифри (циклічно 0..9) */
    if (s->has_current && dir == s->dir) {
        s->current = (uint8_t)((s->current + 1) % 10);
        return SAFE_EV_DIGIT_CHANGED;
    }

    /* Зміна напрямку: підтвердити попередню цифру */
    if (s->has_current) {
        s->entered[s->entered_count++] = s->current;
        s->has_current = false;

        if (s->entered_count == s->code_len) {
            return check_code(s);
        }
    }

    /* Почати нову цифру з 0 */
    s->current = 0;
    s->has_current = true;
    s->dir = (int8_t)dir;
    return SAFE_EV_DIGIT_STARTED;
}

safe_event_t safe_on_button(safe_t *s)
{
    switch (s->state) {
    case SAFE_ST_ENTERING:
        return consume_attempt(s, SAFE_EV_RESET);

    case SAFE_ST_OPEN:
        s->state = SAFE_ST_ENTERING;
        clear_input(s);
        return SAFE_EV_CLOSED;

    case SAFE_ST_LOCKED_OUT:
    default:
        return SAFE_EV_NONE;
    }
}
