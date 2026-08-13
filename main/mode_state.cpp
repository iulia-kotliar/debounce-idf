#include "mode.h"
#include "hw.h"
#include "stats.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

static bool btn_state = false;
static QueueHandle_t evt_q = nullptr;
static const char *TAG = "state-based";

static void IRAM_ATTR isr_fn(void *arg) {
    statsRawEdge();
    int64_t ts = esp_timer_get_time();
    BaseType_t hp = pdFALSE;
    xQueueSendFromISR(evt_q, &ts, &hp);
    if (hp) portYIELD_FROM_ISR();                 
}

static void enter() {
    evt_q = xQueueCreate(32, sizeof(int64_t));
        gpio_config_t cfg = {};
            cfg.pin_bit_mask = (1ULL << PIN_BTN);
            cfg.mode         = GPIO_MODE_INPUT;
            cfg.pull_up_en   = g_pullup;
            cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
            cfg.intr_type    = GPIO_INTR_NEGEDGE;
            gpio_config(&cfg);
    btn_state = (gpio_get_level(PIN_BTN) == BTN_PRESSED);
    gpio_isr_handler_add(PIN_BTN, isr_fn, (void*) PIN_BTN);
}

static void tick() {
    int64_t ts;
    while (xQueueReceive(evt_q, &ts, 0) == pdTRUE) {
        // черга дренується мовчки: події потрібні лише як сигнал
    }

    bool now = (gpio_get_level(PIN_BTN) == BTN_PRESSED);
    if (now == btn_state) return;

    btn_state = now;

    if (now) {
        statsReaction();
        ESP_LOGI(TAG, "реакцій: %u", (unsigned)statsReactions());
    }
}

static void exit_() {
    gpio_isr_handler_remove(PIN_BTN);
    if (evt_q) { vQueueDelete(evt_q); evt_q = nullptr; }
}

const Mode mode_state = { "state-based", enter, exit_, tick };