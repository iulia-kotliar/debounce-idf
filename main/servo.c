#include "servo.h"

#include <stdint.h>
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"

#define SERVO_TIMER         LEDC_TIMER_0
#define SERVO_CHANNEL       LEDC_CHANNEL_0
#define SERVO_FREQ_HZ       50
#define SERVO_RES_BITS      LEDC_TIMER_14_BIT
#define SERVO_MAX_DUTY      ((1U << SERVO_RES_BITS) - 1)
#define SERVO_PERIOD_US     (1000000U / SERVO_FREQ_HZ)  
#define SERVO_MIN_US        500U                        
#define SERVO_MAX_US        2500U                      
static const char *TAG = "SERVO";

void servo_init(gpio_num_t pin)
{
    ledc_timer_config_t timer_cfg = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = SERVO_TIMER,
        .duty_resolution = SERVO_RES_BITS,
        .freq_hz         = SERVO_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    ledc_channel_config_t ch_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = SERVO_CHANNEL,
        .timer_sel  = SERVO_TIMER,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = pin,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_cfg));
}

void servo_set_angle(int deg)
{
    if (deg < SERVO_MIN_DEG) deg = SERVO_MIN_DEG;
    if (deg > SERVO_MAX_DEG) deg = SERVO_MAX_DEG;

    uint32_t us   = SERVO_MIN_US + (uint32_t)deg * (SERVO_MAX_US - SERVO_MIN_US) / 180U;
    uint32_t duty = us * SERVO_MAX_DUTY / SERVO_PERIOD_US;

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, SERVO_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, SERVO_CHANNEL));

    ESP_LOGD(TAG, "%d deg -> %lu us -> duty %lu", deg, (unsigned long)us, (unsigned long)duty);
}
