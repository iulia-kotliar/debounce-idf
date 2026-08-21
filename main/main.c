#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "esp_log.h"

static const char *TAG = "traffic";

/* ======================================================================
 *  Конфігурація
 * ====================================================================== */

#define PIN_RED     GPIO_NUM_4
#define PIN_YELLOW  GPIO_NUM_5
#define PIN_GREEN   GPIO_NUM_6
#define PIN_BUTTON  GPIO_NUM_7   /* до GND, підтяжка вгору вбудована */

#define TICK_MS           100    /* період системного тіка */
#define BLINK_HALF_TICKS  5      /* 5 * 100 мс = 500 мс -> 1 Гц */
#define DEBOUNCE_US       200000 /* 200 мс антидребезгу кнопки */

/* Біти маски ламп */
#define L_RED     (1u << 0)
#define L_YELLOW  (1u << 1)
#define L_GREEN   (1u << 2)

/* Порядок збігається з номерами бітів маски */
static const gpio_num_t lamp_pins[3] = { PIN_RED, PIN_YELLOW, PIN_GREEN };

/* ======================================================================
 *  Стани та таблиця переходів
 * ====================================================================== */

typedef enum {
    ST_GREEN = 0,
    ST_GREEN_BLINK,
    ST_YELLOW,
    ST_RED,
    ST_RED_YELLOW,
    ST_YELLOW_BLINK,
    ST_COUNT
} state_t;

typedef struct {
    const char *name;      /* для логів */
    uint8_t     mask;      /* які лампи горять */
    uint16_t    duration;  /* тривалість у тіках; 0 = нескінченно */
    bool        blink;     /* чи миготить */
    state_t     next;      /* куди йдемо після duration */
} state_row_t;

/*
 * Designated initializers: порядок рядків у масиві не має значення,
 * кожен рядок прив'язаний до свого enum-значення.
 */
static const state_row_t table[ST_COUNT] = {
    [ST_GREEN]        = { "GREEN",        L_GREEN,          60, false, ST_GREEN_BLINK  },
    [ST_GREEN_BLINK]  = { "GREEN_BLINK",  L_GREEN,          30, true,  ST_YELLOW       },
    [ST_YELLOW]       = { "YELLOW",       L_YELLOW,         30, false, ST_RED          },
    [ST_RED]          = { "RED",          L_RED,            60, false, ST_RED_YELLOW   },
    [ST_RED_YELLOW]   = { "RED_YELLOW",   L_RED | L_YELLOW, 20, false, ST_GREEN        },
    [ST_YELLOW_BLINK] = { "YELLOW_BLINK", L_YELLOW,          0, true,  ST_YELLOW_BLINK },
};

/* ======================================================================
 *  Події
 * ====================================================================== */

typedef enum {
    EV_TICK = 0,
    EV_BUTTON
} event_t;

static QueueHandle_t evt_q;

/* ======================================================================
 *  Змінні автомата
 * ====================================================================== */

static state_t  state;
static uint16_t ticks_in_state;

/* ======================================================================
 *  Ядро автомата
 * ====================================================================== */

/*
 * Єдина точка зміни стану. Ніде більше не можна писати в `state` напряму,
 * інакше рано чи пізно забудеться скидання лічильника.
 */
static void enter_state(state_t s)
{
    state = s;
    ticks_in_state = 0;
    ESP_LOGI(TAG, "-> %s", table[s].name);
}

/* Функція виходу: стан (+ фаза миготіння) -> три GPIO */
static void apply_output(void)
{
    uint8_t mask = table[state].mask;

    if (table[state].blink && ((ticks_in_state / BLINK_HALF_TICKS) & 1u)) {
        mask = 0;
    }

    for (int i = 0; i < 3; i++) {
        gpio_set_level(lamp_pins[i], (mask >> i) & 1u);
    }
}

static void fsm_handle(event_t ev)
{
    if (ev == EV_BUTTON) {
        /* Правило кнопки однакове для всіх станів, тому воно поза таблицею.
         * Повертаємось завжди в RED — найбезпечніший стан для водіїв. */
        enter_state(state == ST_YELLOW_BLINK ? ST_RED : ST_YELLOW_BLINK);
    } else {
        ticks_in_state++;

        uint16_t dur = table[state].duration;
        if (dur != 0 && ticks_in_state >= dur) {
            enter_state(table[state].next);
        }
    }

    apply_output();
}

/* ======================================================================
 *  Джерела подій
 * ====================================================================== */

/* Викликається з задачі esp_timer, не з ISR */
static void tick_cb(void *arg)
{
    event_t ev = EV_TICK;
    xQueueSend(evt_q, &ev, 0);
}

static void IRAM_ATTR button_isr(void *arg)
{
    static int64_t last_us = 0;
    int64_t now = esp_timer_get_time();

    if (now - last_us < DEBOUNCE_US) {
        return;
    }
    last_us = now;

    event_t ev = EV_BUTTON;
    BaseType_t hp_task_woken = pdFALSE;
    xQueueSendFromISR(evt_q, &ev, &hp_task_woken);
    if (hp_task_woken) {
        portYIELD_FROM_ISR();
    }
}

static void fsm_task(void *arg)
{
    event_t ev;
    for (;;) {
        if (xQueueReceive(evt_q, &ev, portMAX_DELAY) == pdTRUE) {
            fsm_handle(ev);
        }
    }
}

/* ======================================================================
 *  Ініціалізація
 * ====================================================================== */

static void gpio_init(void)
{
    gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << PIN_RED) |
                        (1ULL << PIN_YELLOW) |
                        (1ULL << PIN_GREEN),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&out_cfg));

    gpio_config_t in_cfg = {
        .pin_bit_mask = (1ULL << PIN_BUTTON),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&in_cfg));

    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_BUTTON, button_isr, NULL));
}

/* Ловить забутий рядок, якщо в enum додали стан, а в таблицю — ні */
static void table_validate(void)
{
    for (int i = 0; i < ST_COUNT; i++) {
        if (table[i].name == NULL) {
            ESP_LOGE(TAG, "у таблиці немає рядка для стану %d", i);
            abort();
        }
    }
}

void app_main(void)
{
    table_validate();
    gpio_init();

    evt_q = xQueueCreate(8, sizeof(event_t));
    ESP_ERROR_CHECK(evt_q ? ESP_OK : ESP_ERR_NO_MEM);

    /* Стартуємо з червоного */
    enter_state(ST_RED);
    apply_output();

    xTaskCreate(fsm_task, "fsm", 3072, NULL, 5, NULL);

    const esp_timer_create_args_t timer_args = {
        .callback = tick_cb,
        .name     = "tick",
    };
    esp_timer_handle_t tick_timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, TICK_MS * 1000));

    ESP_LOGI(TAG, "світлофор запущено, тік = %d мс", TICK_MS);
}