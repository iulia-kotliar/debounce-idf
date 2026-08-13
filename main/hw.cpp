#include "hw.h"
#include "stats.h"

#include "esp_log.h"

static const char *TAG = "hw";

gpio_pullup_t g_pullup = GPIO_PULLUP_ENABLE;

void hwSetPullup(bool enabled) {
    g_pullup = enabled ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
}

bool hwPullupEnabled() {
    return g_pullup == GPIO_PULLUP_ENABLE;
}

// Черга, у яку пише обробник. Файлова змінна, а не аргумент: ISR отримує
// контекст через void*, але зберігати його все одно нема де, а режим
// одночасно завжди рівно один.
static QueueHandle_t edge_q = nullptr;

static void IRAM_ATTR edgeIsr(void *arg) {
    statsRawEdge();

    int64_t ts = esp_timer_get_time();
    BaseType_t hp = pdFALSE;
    xQueueSendFromISR(edge_q, &ts, &hp);
    if (hp) portYIELD_FROM_ISR();
}

static esp_err_t configButton(gpio_int_type_t intr) {
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = (1ULL << PIN_BTN);
    cfg.mode         = GPIO_MODE_INPUT;
    cfg.pull_up_en   = g_pullup;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type    = intr;
    return gpio_config(&cfg);
}

esp_err_t hwConfigButtonPolling() {
    esp_err_t err = configButton(GPIO_INTR_DISABLE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config: %s", esp_err_to_name(err));
    }
    return err;
}

QueueHandle_t edgeSourceStart() {
    edge_q = xQueueCreate(32, sizeof(int64_t));
    if (!edge_q) {
        ESP_LOGE(TAG, "не вдалося створити чергу подій");
        return nullptr;
    }

    // Пін конфігурується вже після створення черги: інакше фронт міг би
    // прийти раніше, ніж їй буде куди писати.
    esp_err_t err = configButton(GPIO_INTR_NEGEDGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config: %s", esp_err_to_name(err));
        vQueueDelete(edge_q);
        edge_q = nullptr;
        return nullptr;
    }

    err = gpio_isr_handler_add(PIN_BTN, edgeIsr, nullptr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_isr_handler_add: %s", esp_err_to_name(err));
        configButton(GPIO_INTR_DISABLE);
        vQueueDelete(edge_q);
        edge_q = nullptr;
        return nullptr;
    }

    return edge_q;
}

void edgeSourceStop(QueueHandle_t &q) {
    // Порядок принциповий: спершу відчепити обробник, інакше фронт може
    // прийти на вже видалену чергу.
    gpio_isr_handler_remove(PIN_BTN);
    configButton(GPIO_INTR_DISABLE);

    if (edge_q) {
        vQueueDelete(edge_q);
        edge_q = nullptr;
    }
    q = nullptr;
}