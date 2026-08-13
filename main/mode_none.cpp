#include "mode.h"
#include "hw.h"
#include "stats.h"
#include "esp_log.h"


static const char *TAG = "none";
static QueueHandle_t evt_q = nullptr;

static void enter() {
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
        statsReaction();
        ESP_LOGI(TAG, "реакцій: %u", (unsigned)statsReactions());
    }
}

const Mode mode_none = { "без debounce", enter, exit_, tick };