#include "buzzer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_err.h"

#define BUZ_TIMER       LEDC_TIMER_1
#define BUZ_CHANNEL     LEDC_CHANNEL_1
#define BUZ_RES_BITS    LEDC_TIMER_10_BIT
#define BUZ_DUTY_ON     (1U << (BUZ_RES_BITS - 1))   /* 50 % - меандр */

void buzzer_init(gpio_num_t pin)
{
    ledc_timer_config_t timer_cfg = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = BUZ_TIMER,
        .duty_resolution = BUZ_RES_BITS,
        .freq_hz         = 2000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    ledc_channel_config_t ch_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = BUZ_CHANNEL,
        .timer_sel  = BUZ_TIMER,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = pin,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_cfg));
}

static void buzzer_off(void)
{
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZ_CHANNEL, 0));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZ_CHANNEL));
}

void buzzer_tone(uint32_t freq_hz, uint32_t duration_ms)
{
    if (freq_hz > 0) {
        ESP_ERROR_CHECK(ledc_set_freq(LEDC_LOW_SPEED_MODE, BUZ_TIMER, freq_hz));
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZ_CHANNEL, BUZ_DUTY_ON));
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZ_CHANNEL));
    }
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    buzzer_off();
}

void buzzer_play(const buzzer_note_t *notes, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        buzzer_tone(notes[i].freq_hz, notes[i].duration_ms);
        vTaskDelay(pdMS_TO_TICKS(20));      /* коротка пауза, щоб ноти не зливались */
    }
}
