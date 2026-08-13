#include "mode.h"
#include "hw.h"
#include "stats.h"
#include "esp_log.h"

static const char *TAG = "state-based";
static QueueHandle_t evt_q = nullptr;

static bool btn_state = false;

static void enter() {
    evt_q = edgeSourceStart();
    if (!evt_q) ESP_LOGE(TAG, "режим не запустився");

    btn_state = (gpio_get_level(PIN_BTN) == BTN_PRESSED);
}

static void exit_() {
    edgeSourceStop(evt_q);
}

static void tick() {
    if (evt_q) {
        int64_t ts;
        while (xQueueReceive(evt_q, &ts, 0) == pdTRUE) {
            // дренуємо мовчки: події потрібні лише як сигнал
        }
    }

    bool now = (gpio_get_level(PIN_BTN) == BTN_PRESSED);
    if (now == btn_state) return;

    btn_state = now;

    if (now) {
        statsReaction();
        ESP_LOGI(TAG, "реакцій: %u", (unsigned)statsReactions());
    }
}

const Mode mode_state = { "state-based", enter, exit_, tick };