#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"

static const char *TAG = "pwm";

#define LED_GPIO            15
#define MOTOR_GPIO          16

#define POT_LED_CHANNEL     ADC_CHANNEL_0   /* GPIO1 */
#define POT_MOTOR_CHANNEL   ADC_CHANNEL_1   /* GPIO2 */

/* ---------- PWM ---------- */
#define PWM_RESOLUTION      LEDC_TIMER_10_BIT
#define PWM_MAX             ((1 << 10) - 1)     /* 1023 */

#define LED_TIMER           LEDC_TIMER_0
#define LED_CHANNEL         LEDC_CHANNEL_0
#define LED_FREQ_HZ         5000               

#define MOTOR_TIMER         LEDC_TIMER_1
#define MOTOR_CHANNEL       LEDC_CHANNEL_1
#define MOTOR_FREQ_HZ       20000              

/* ---------- АЦП ---------- */
#define ADC_MAX             4095
#define ADC_SAMPLES         16                 

/* ---------- Двигун ---------- */
#define MOTOR_DEAD_ZONE     40               
#define MOTOR_MIN_DUTY      350               

static adc_oneshot_unit_handle_t s_adc1;


static void pwm_init(void)
{
    const ledc_timer_config_t led_timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = LED_TIMER,
        .duty_resolution = PWM_RESOLUTION,
        .freq_hz         = LED_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&led_timer));

    const ledc_timer_config_t motor_timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = MOTOR_TIMER,
        .duty_resolution = PWM_RESOLUTION,
        .freq_hz         = MOTOR_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&motor_timer));

    const ledc_channel_config_t led_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LED_CHANNEL,
        .timer_sel  = LED_TIMER,
        .gpio_num   = LED_GPIO,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&led_channel));

    const ledc_channel_config_t motor_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = MOTOR_CHANNEL,
        .timer_sel  = MOTOR_TIMER,
        .gpio_num   = MOTOR_GPIO,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&motor_channel));
}

static void adc_init(void)
{
    const adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc1));

    const adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten    = ADC_ATTEN_DB_12,   
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc1, POT_LED_CHANNEL, &chan_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc1, POT_MOTOR_CHANNEL, &chan_cfg));
}

static int adc_read_avg(adc_channel_t channel)
{
    int sum = 0;
    for (int i = 0; i < ADC_SAMPLES; i++) {
        int raw = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(s_adc1, channel, &raw));
        sum += raw;
    }
    return sum / ADC_SAMPLES;
}

static void set_duty(ledc_channel_t channel, uint32_t duty)
{
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, channel));
}

static uint32_t led_duty_from_adc(int raw)
{
    uint32_t linear = (uint32_t)raw * PWM_MAX / ADC_MAX;
    return linear * linear / PWM_MAX;
}

static uint32_t motor_duty_from_adc(int raw)
{
    if (raw < MOTOR_DEAD_ZONE) {
        return 0;
    }

    uint32_t span  = ADC_MAX - MOTOR_DEAD_ZONE;
    uint32_t value = (uint32_t)(raw - MOTOR_DEAD_ZONE);

    return MOTOR_MIN_DUTY + value * (PWM_MAX - MOTOR_MIN_DUTY) / span;
}

void app_main(void)
{
    pwm_init();
    adc_init();

    ESP_LOGI(TAG, "LED: GPIO%d @ %d Hz, timer %d", LED_GPIO, LED_FREQ_HZ, LED_TIMER);
    ESP_LOGI(TAG, "Motor: GPIO%d @ %d Hz, timer %d", MOTOR_GPIO, MOTOR_FREQ_HZ, MOTOR_TIMER);

    while (1) {
        int led_raw   = adc_read_avg(POT_LED_CHANNEL);
        int motor_raw = adc_read_avg(POT_MOTOR_CHANNEL);

        uint32_t led_duty   = led_duty_from_adc(led_raw);
        uint32_t motor_duty = motor_duty_from_adc(motor_raw);

        set_duty(LED_CHANNEL, led_duty);
        set_duty(MOTOR_CHANNEL, motor_duty);

        ESP_LOGI(TAG, "LED  adc=%4d duty=%4lu (%3lu%%) | MOTOR adc=%4d duty=%4lu (%3lu%%)",
                 led_raw,   (unsigned long)led_duty,   (unsigned long)(led_duty   * 100 / PWM_MAX),
                 motor_raw, (unsigned long)motor_duty, (unsigned long)(motor_duty * 100 / PWM_MAX));

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}