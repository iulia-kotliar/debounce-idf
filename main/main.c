#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "fan";


#define FAN_GPIO        GPIO_NUM_5
#define LED_RUN_GPIO    GPIO_NUM_6
#define LED_IDLE_GPIO   GPIO_NUM_7

#define FAN_LEVEL_ON    1
#define FAN_LEVEL_OFF   0

#define LED_LEVEL_ON    1
#define LED_LEVEL_OFF   0


#define DEBUG_TIMINGS   1

#if DEBUG_TIMINGS
    #define FAN_PERIOD_MS   20000UL                 /* 20 c   */
    #define FAN_RUN_MS       5000UL                 /*  5 c   */
#else
    #define FAN_PERIOD_MS   (60UL * 60UL * 1000UL)  /* 60 хв  */
    #define FAN_RUN_MS      (15UL * 60UL * 1000UL)  /* 15 хв  */
#endif

#define MS_TO_US(ms)    ((uint64_t)(ms) * 1000ULL)

_Static_assert(FAN_RUN_MS < FAN_PERIOD_MS,
               "Час роботи має бути меншим за період циклу");


#define HEALTH_PERIOD_MS    1000UL   /* період опитування наглядача       */
#define FAN_RUN_GRACE_MS    1000UL   /* допуск понад FAN_RUN_MS до аварії */

/* Штучна поломка для демонстрації спрацювання watchdog. */
#define WDT_SABOTAGE_DEMO   0

typedef enum {
    FAN_IDLE = 0,
    FAN_RUNNING,
} fan_state_t;

static volatile fan_state_t s_state = FAN_IDLE;
static volatile int64_t     s_run_started_us = 0;
static uint32_t             s_cycle_count = 0;

static esp_timer_handle_t s_cycle_timer = NULL;  /* періодичний: старт циклу   */
static esp_timer_handle_t s_run_timer   = NULL;  /* одноразовий: кінець роботи */

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

static inline uint32_t uptime_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static const char *state_name(fan_state_t st)
{
    return (st == FAN_RUNNING) ? "RUNNING" : "IDLE";
}

static void fan_hw_init(void)
{
    gpio_set_level(FAN_GPIO,      FAN_LEVEL_OFF);
    gpio_set_level(LED_RUN_GPIO,  LED_LEVEL_OFF);
    gpio_set_level(LED_IDLE_GPIO, LED_LEVEL_OFF);

    const gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << FAN_GPIO)
                      | (1ULL << LED_RUN_GPIO)
                      | (1ULL << LED_IDLE_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));

    gpio_set_level(FAN_GPIO,      FAN_LEVEL_OFF);
    gpio_set_level(LED_RUN_GPIO,  LED_LEVEL_OFF);
    gpio_set_level(LED_IDLE_GPIO, LED_LEVEL_ON); 

    ESP_LOGI(TAG, "GPIO ініціалізовано: реле=%d, LED run=%d, LED idle=%d",
             FAN_GPIO, LED_RUN_GPIO, LED_IDLE_GPIO);
}

static void fan_set(bool on)
{
    gpio_set_level(FAN_GPIO,      on ? FAN_LEVEL_ON  : FAN_LEVEL_OFF);
    gpio_set_level(LED_RUN_GPIO,  on ? LED_LEVEL_ON  : LED_LEVEL_OFF);
    gpio_set_level(LED_IDLE_GPIO, on ? LED_LEVEL_OFF : LED_LEVEL_ON);
}

static void on_cycle_start(void *arg)
{
    bool start = false;

    portENTER_CRITICAL(&s_lock);
    if (s_state == FAN_IDLE && !esp_timer_is_active(s_run_timer)) {
        s_state = FAN_RUNNING;
        s_run_started_us = esp_timer_get_time();
        start = true;
    }
    portEXIT_CRITICAL(&s_lock);

    if (!start) {
        ESP_LOGW(TAG, "[%lu ms] цикл пропущено — стан %s, вентилятор ще працює",
                 uptime_ms(), state_name(s_state));
        return;
    }

    s_cycle_count++;
    fan_set(true);

    ESP_LOGI(TAG, "[%lu ms] цикл #%lu: ON (на %lu ms)",
             uptime_ms(), s_cycle_count, FAN_RUN_MS);

    ESP_ERROR_CHECK(esp_timer_start_once(s_run_timer, MS_TO_US(FAN_RUN_MS)));
}

static void on_run_end(void *arg)
{
    portENTER_CRITICAL(&s_lock);
    s_state = FAN_IDLE;
    portEXIT_CRITICAL(&s_lock);

    fan_set(false);

    ESP_LOGI(TAG, "[%lu ms] цикл #%lu: OFF (пауза до наступного циклу)",
             uptime_ms(), s_cycle_count);
}

static void supervise(void)
{
    if (!esp_timer_is_active(s_cycle_timer)) {
        ESP_LOGE(TAG, "[%lu ms] АВАРІЯ: періодичний таймер зупинився, рестарт",
                 uptime_ms());
        esp_timer_start_periodic(s_cycle_timer, MS_TO_US(FAN_PERIOD_MS));
    }

    bool force_off = false;

    portENTER_CRITICAL(&s_lock);
    if (s_state == FAN_RUNNING) {
        int64_t elapsed_ms = (esp_timer_get_time() - s_run_started_us) / 1000;
        if (elapsed_ms > (int64_t)(FAN_RUN_MS + FAN_RUN_GRACE_MS)) {
            s_state = FAN_IDLE;
            force_off = true;
        }
    }
    portEXIT_CRITICAL(&s_lock);

    if (force_off) {
        esp_timer_stop(s_run_timer);
        fan_set(false);
        ESP_LOGE(TAG, "[%lu ms] АВАРІЯ: перевищено час роботи, примусове OFF",
                 uptime_ms());
    }
}

static void health_task(void *arg)
{
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    ESP_LOGI(TAG, "наглядову задачу підписано на TWDT");

    while (1) {
        esp_task_wdt_reset();
        supervise();

#if WDT_SABOTAGE_DEMO
        if (uptime_ms() > 30000) {
            ESP_LOGE(TAG, "SABOTAGE: зависаємо навмисно, чекаємо TWDT");
            while (1) { /* більше не годуємо watchdog */ }
        }
#endif

        vTaskDelay(pdMS_TO_TICKS(HEALTH_PERIOD_MS));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "старт: період %lu ms, час роботи %lu ms",
             FAN_PERIOD_MS, FAN_RUN_MS);

    fan_hw_init();

    const esp_timer_create_args_t cycle_args = {
        .callback        = &on_cycle_start,
        .arg             = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name            = "fan_cycle",
    };
    ESP_ERROR_CHECK(esp_timer_create(&cycle_args, &s_cycle_timer));

    const esp_timer_create_args_t run_args = {
        .callback        = &on_run_end,
        .arg             = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name            = "fan_run",
    };
    ESP_ERROR_CHECK(esp_timer_create(&run_args, &s_run_timer));

    ESP_ERROR_CHECK(esp_timer_start_periodic(s_cycle_timer,
                                             MS_TO_US(FAN_PERIOD_MS)));

    on_cycle_start(NULL);

    xTaskCreate(health_task, "health", 3072, NULL, 3, NULL);

    ESP_LOGI(TAG, "ініціалізацію завершено, app_main виходить");
}