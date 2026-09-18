#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "adc_cali";

#define ADC_UNIT_ID        ADC_UNIT_1
#define ADC_CHAN           ADC_CHANNEL_3      
#define ADC_ATTEN          ADC_ATTEN_DB_12    
#define ADC_BITS           ADC_BITWIDTH_12    
#define ADC_RAW_MAX        4095

#define U_FULL_SCALE_MV    3100.0f

#define SAMPLE_PERIOD_MS   100   
#define OVERSAMPLE         8    

#define PRINT_MIN_DELTA    50

#define HEADER_EVERY       20   
#define OUTPUT_CSV         1    

static bool cali_init(adc_cali_handle_t *out_handle)
{
    esp_err_t ret = ESP_ERR_NOT_SUPPORTED;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cfg = {
        .unit_id  = ADC_UNIT_ID,
        .chan     = ADC_CHAN,
        .atten    = ADC_ATTEN,
        .bitwidth = ADC_BITS,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cfg, out_handle);
#endif

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration scheme: curve fitting (eFuse)");
        return true;
    }
    if (ret == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "eFuse calibration data not available on this chip");
    } else {
        ESP_LOGE(TAG, "Calibration init failed: %s", esp_err_to_name(ret));
    }
    return false;
}

static int read_raw_avg(adc_oneshot_unit_handle_t adc)
{
    int sum = 0;
    for (int i = 0; i < OVERSAMPLE; i++) {
        int raw = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(adc, ADC_CHAN, &raw));
        sum += raw;
    }
    return (sum + OVERSAMPLE / 2) / OVERSAMPLE;   /* округлення */
}

static void print_config(bool cali_ok)
{
    printf("\n=============== ADC config ===============\n");
    printf("Chip          : ESP32-S3\n");
    printf("Unit / Chan   : ADC1 / CH3 (GPIO4)\n");
    printf("Resolution    : 12 bit (0..%d)\n", ADC_RAW_MAX);
    printf("Attenuation   : 12 dB (~0..3100 mV)\n");
    printf("Vref          : ~1100 mV internal (per-chip, stored in eFuse)\n");
    printf("Manual formula: U = RAW * %.0f / %d\n", U_FULL_SCALE_MV, ADC_RAW_MAX);
    printf("Calibration   : %s\n", cali_ok ? "curve fitting" : "NOT AVAILABLE");
    printf("Period        : %d ms, oversample x%d\n", SAMPLE_PERIOD_MS, OVERSAMPLE);
    printf("==========================================\n");
}

static void print_header(void)
{
#if OUTPUT_CSV
    printf("raw,u_manual_mv,u_cali_mv,error_pct\n");
#else
    printf("\n RAW   U_manual(mV)   U_cali(mV)   Error(%%)\n");
    printf("--------------------------------------------\n");
#endif
}

static void print_row(int raw, float u_manual, int u_cali, bool cali_ok)
{
    bool err_valid = cali_ok && u_cali > 0;
    float err = err_valid ? (u_manual - u_cali) * 100.0f / u_cali : 0.0f;

#if OUTPUT_CSV
    if (err_valid) {
        printf("%d,%.1f,%d,%.2f\n", raw, u_manual, u_cali, err);
    } else {
        printf("%d,%.1f,%d,\n", raw, u_manual, cali_ok ? u_cali : -1);
    }
#else
    if (!cali_ok) {
        printf("%4d   %12.1f   %10s   %8s\n", raw, u_manual, "n/a", "n/a");
    } else if (!err_valid) {
        printf("%4d   %12.1f   %10d   %8s\n", raw, u_manual, u_cali, "n/a");
    } else {
        printf("%4d   %12.1f   %10d   %8.2f\n", raw, u_manual, u_cali, err);
    }
#endif
}

void app_main(void)
{
    adc_oneshot_unit_handle_t adc = NULL;
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_ID,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = ADC_ATTEN,
        .bitwidth = ADC_BITS,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc, ADC_CHAN, &chan_cfg));

    adc_cali_handle_t cali = NULL;
    bool cali_ok = cali_init(&cali);

    print_config(cali_ok);
    print_header();

    int rows = 0;
    int last_printed_raw = -10000;
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        int raw = read_raw_avg(adc);

        float u_manual = raw * U_FULL_SCALE_MV / ADC_RAW_MAX;

        int u_cali = 0;
        if (cali_ok) {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali, raw, &u_cali));
        }

        if (abs(raw - last_printed_raw) >= PRINT_MIN_DELTA) {
#if !OUTPUT_CSV
            if (rows > 0 && rows % HEADER_EVERY == 0) {
                print_header();
            }
#endif
            print_row(raw, u_manual, u_cali, cali_ok);
            last_printed_raw = raw;
            rows++;
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}