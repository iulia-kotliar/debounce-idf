#include "mode.h"
#include "hw.h"
#include "stats.h"

#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "polling";

enum class St : uint8_t { Released, MaybePress, Pressed, MaybeRelease };

static St       state       = St::Released;
static uint32_t last_poll   = 0;
static uint32_t state_since = 0;

static constexpr uint32_t POLL_MS   = 5;
static constexpr uint32_t STABLE_MS = 20;

static void enter() {
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = (1ULL << PIN_BTN);
    cfg.mode         = GPIO_MODE_INPUT;
    cfg.pull_up_en   = g_pullup;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type    = GPIO_INTR_DISABLE;
    gpio_config(&cfg);

    uint32_t now = millis();
    last_poll    = now;
    state_since  = now;
    state        = (gpio_get_level(PIN_BTN) == BTN_PRESSED) ? St::Pressed : St::Released;
}

static void exit_() {
    // Переривань не вішали — знімати нічого.
}

static void tick() {
    uint32_t now = millis();
    if (now - last_poll < POLL_MS) return;
    last_poll = now;

    bool pressed = (gpio_get_level(PIN_BTN) == BTN_PRESSED);

    switch (state) {
        case St::Released:
            if (pressed) {
                state = St::MaybePress;
                state_since = now;
            }
            break;

        case St::MaybePress:
            if (!pressed) {
                state = St::Released;               // відскок, натискання не було
            } else if (now - state_since >= STABLE_MS) {
                state = St::Pressed;                // рівень устоявся — це натискання
                statsReaction();
                ESP_LOGI(TAG, "реакцій: %u", (unsigned)statsReactions());
            }
            break;

        case St::Pressed:
            if (!pressed) {
                state = St::MaybeRelease;
                state_since = now;
            }
            break;

        case St::MaybeRelease:
            if (pressed) {
                state = St::Pressed;                // відскок під час утримання
            } else if (now - state_since >= STABLE_MS) {
                state = St::Released;               // відпускання підтверджене
            }
            break;
    }
}

const Mode mode_poll = { "polling+FSM", enter, exit_, tick };