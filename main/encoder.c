#include "encoder.h"

#include <stddef.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_attr.h"
#include "esp_err.h"

#define BTN_DEBOUNCE_MS     30

static gpio_num_t s_clk;
static gpio_num_t s_dt;
static gpio_num_t s_sw;

static volatile int32_t s_steps = 0;
static volatile uint8_t s_state = 0;
static int32_t s_acc = 0;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

/* Таблиця квадратурного декодування: індекс = (попередній AB << 2) | новий AB.
 * Неможливі переходи (брязкіт, пропущений фронт) дають 0. */
static const int8_t QDEC_TABLE[16] = {
     0, -1, +1,  0,
    +1,  0,  0, -1,
    -1,  0,  0, +1,
     0, +1, -1,  0
};

static void IRAM_ATTR encoder_isr(void *arg)
{
    (void)arg;
    uint8_t ab = (uint8_t)((gpio_get_level(s_clk) << 1) | gpio_get_level(s_dt));

    portENTER_CRITICAL_ISR(&s_mux);
    s_state = (uint8_t)(((s_state << 2) | ab) & 0x0F);
    s_steps += QDEC_TABLE[s_state];
    portEXIT_CRITICAL_ISR(&s_mux);
}

void encoder_init(gpio_num_t clk, gpio_num_t dt, gpio_num_t sw)
{
    s_clk = clk;
    s_dt  = dt;
    s_sw  = sw;

    gpio_config_t enc_cfg = {
        .pin_bit_mask = (1ULL << clk) | (1ULL << dt),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_ANYEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&enc_cfg));

    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << sw),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,     /* на SW модуля немає підтяжки */
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&btn_cfg));

    s_state = (uint8_t)((gpio_get_level(clk) << 1) | gpio_get_level(dt));

    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(clk, encoder_isr, NULL));
    ESP_ERROR_CHECK(gpio_isr_handler_add(dt, encoder_isr, NULL));
}

int encoder_take_detents(void)
{
    portENTER_CRITICAL(&s_mux);
    int32_t steps = s_steps;
    s_steps = 0;
    portEXIT_CRITICAL(&s_mux);

    s_acc += steps;
    int detents = (int)(s_acc / ENCODER_STEPS_PER_DETENT);
    s_acc -= detents * ENCODER_STEPS_PER_DETENT;
    return detents;
}

void encoder_discard(void)
{
    portENTER_CRITICAL(&s_mux);
    s_steps = 0;
    portEXIT_CRITICAL(&s_mux);
    s_acc = 0;
}

void encoder_disable(void)
{
    gpio_isr_handler_remove(s_clk);
    gpio_isr_handler_remove(s_dt);
    encoder_discard();
}

bool encoder_button_pressed(void)
{
    static int stable = 1;
    static int last_raw = 1;
    static TickType_t changed_at = 0;

    int raw = gpio_get_level(s_sw);
    TickType_t now = xTaskGetTickCount();

    if (raw != last_raw) {
        last_raw = raw;
        changed_at = now;
    }
    if (raw != stable && (now - changed_at) >= pdMS_TO_TICKS(BTN_DEBOUNCE_MS)) {
        stable = raw;
        return stable == 0;     /* натиснута = 0 */
    }
    return false;
}
