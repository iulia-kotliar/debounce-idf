#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

static const char *TAG = "servo_pot";

#define SERVO_GPIO          5
#define SERVO_FREQ_HZ       50
#define SERVO_PERIOD_US     20000
#define LEDC_RES_BITS       14
#define SERVO_MIN_US        500     
#define SERVO_MAX_US        2400   
#define SERVO_RANGE_DEG     180
#define SERVO_INVERT        0       

#define POT_ADC_UNIT        ADC_UNIT_1
#define POT_ADC_CHANNEL     ADC_CHANNEL_0    
#define POT_ADC_ATTEN       ADC_ATTEN_DB_12  
#define POT_TRAVEL_DEG      270     
#define POT_VCC_MV          3300    
#define ADC_SAMPLES         16      

#define LOOP_PERIOD_MS      20
#define HYSTERESIS_X10      7      

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali;
static bool s_cali_ok;

static void servo_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_RES_BITS,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = SERVO_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t ch = {
        .gpio_num   = SERVO_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch));
}

static void servo_write_x10(int angle_x10)
{
    if (angle_x10 < 0)                    angle_x10 = 0;
    if (angle_x10 > SERVO_RANGE_DEG * 10) angle_x10 = SERVO_RANGE_DEG * 10;
#if SERVO_INVERT
    angle_x10 = SERVO_RANGE_DEG * 10 - angle_x10;
#endif
    uint32_t pulse_us = SERVO_MIN_US +
        (uint32_t)(SERVO_MAX_US - SERVO_MIN_US) * angle_x10 / (SERVO_RANGE_DEG * 10);
    uint32_t duty = (pulse_us * (1U << LEDC_RES_BITS)) / SERVO_PERIOD_US;

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
}

static void pot_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = { .unit_id = POT_ADC_UNIT };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc));

    adc_oneshot_chan_cfg_t ch_cfg = {
        .atten    = POT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc, POT_ADC_CHANNEL, &ch_cfg));

    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id  = POT_ADC_UNIT,
        .chan     = POT_ADC_CHANNEL,
        .atten    = POT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    s_cali_ok = (adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali) == ESP_OK);
    ESP_LOGI(TAG, "ADC calibration: %s", s_cali_ok ? "curve fitting" : "not available, raw fallback");
}

static int pot_read_mv(void)
{
    int sum = 0;
    for (int i = 0; i < ADC_SAMPLES; i++) {
        int raw = 0, mv = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(s_adc, POT_ADC_CHANNEL, &raw));
        if (s_cali_ok) {
            adc_cali_raw_to_voltage(s_cali, raw, &mv);
        } else {
            mv = raw * 3100 / 4095;   // груба оцінка без калібрування
        }
        sum += mv;
    }
    return sum / ADC_SAMPLES;
}

static int pot_angle_x10(int mv)
{
    return (int)((int64_t)mv * POT_TRAVEL_DEG * 10 / POT_VCC_MV);
}

void app_main(void)
{
    servo_init();
    pot_init();

    ESP_LOGI(TAG, "Pot travel %d deg, servo range %d deg -> working range 0..%d deg",
             POT_TRAVEL_DEG, SERVO_RANGE_DEG, SERVO_RANGE_DEG);

    int last_x10 = -1000;   

    while (1) {
        int mv      = pot_read_mv();
        int pot_x10 = pot_angle_x10(mv);

        bool clipped = pot_x10 > SERVO_RANGE_DEG * 10;
        int angle_x10 = clipped ? SERVO_RANGE_DEG * 10 : pot_x10;

        if (abs(angle_x10 - last_x10) >= HYSTERESIS_X10 ||
            (clipped && last_x10 != SERVO_RANGE_DEG * 10)) {
            last_x10 = angle_x10;
            servo_write_x10(angle_x10);
            ESP_LOGI(TAG, "angle from left: %3d.%d deg  (pot %4d mV)%s",
                     angle_x10 / 10, angle_x10 % 10, mv,
                     clipped ? "  [limit]" : "");
        }

        vTaskDelay(pdMS_TO_TICKS(LOOP_PERIOD_MS));
    }
}