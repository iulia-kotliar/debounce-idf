#pragma once
#include <stdint.h>

struct Mode {
    const char* name;
    void (*enter)();
    void (*exit)();
    void (*tick)();
};

extern const Mode mode_none;    // завдання 1: без debounce
extern const Mode mode_time;    // завдання 2: time-based, поза ISR
extern const Mode mode_state;   // завдання 3: state-based, перевірка рівня
extern const Mode mode_poll;    // завдання 4: polling + FSM, без переривань
