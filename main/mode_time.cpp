#include "mode.h"
#include "hw.h"
#include "stats.h"

#include "esp_log.h"

// Завдання 2: time-based debounce поза ISR.
// Подія приймається, якщо від попередньої прийнятої минуло >= 50 мс.
// Порівнюються мітки часу самих фронтів, а не момент обробки в tick(),
// тому вікно відлічується так, як цього вимагає визначення методу.

static const char *TAG = "time-based";
static QueueHandle_t evt_q = nullptr;

static int64_t lastEvent = 0;
static constexpr int64_t DEBOUNCE_US = 50000;   // 50 мс

static void enter() {
    lastEvent = 0;
    evt_q = edgeSourceStart();
    if (!evt_q) ESP_LOGE(TAG, "режим не запустився");
}

static void exit_() {
    edgeSourceStop(evt_q);
}

static void tick() {
    if (!evt_q) return;

    int64_t ts;
    while (xQueueReceive(evt_q, &ts, 0) == pdTRUE) {
        if (ts - lastEvent >= DEBOUNCE_US) {
            lastEvent = ts;
            statsReaction();
            ESP_LOGI(TAG, "реакцій: %u", (unsigned)statsReactions());
        }
    }
}

const Mode mode_time = { "time-based", enter, exit_, tick };