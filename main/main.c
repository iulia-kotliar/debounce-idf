#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include "sma.h"

#define THRESHOLD_ON   300 
#define THRESHOLD_OFF  450 

typedef enum { LED_OFF = 0, LED_ON = 1 } led_state_t;

led_state_t hysteresis_update(led_state_t state, uint16_t avg) {
    switch (state) {
    case LED_OFF:
        if (avg < THRESHOLD_ON) return LED_ON;
        break;
    case LED_ON:
        if (avg > THRESHOLD_OFF) return LED_OFF;
        break;
    }
    return state;   // між порогами — нічого не змінюємо
}

void app_main(void) {
    adc_oneshot_unit_handle_t adc;
    adc_oneshot_unit_init_cfg_t ucfg = { .unit_id = ADC_UNIT_1 };
    adc_oneshot_new_unit(&ucfg, &adc);
    adc_oneshot_chan_cfg_t ccfg = { .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT };
    adc_oneshot_config_channel(adc, ADC_CHANNEL_3, &ccfg);   // GPIO4

    gpio_reset_pin(GPIO_NUM_16);
    gpio_set_direction(GPIO_NUM_16, GPIO_MODE_OUTPUT);

    sma_t sma;
    sma_init(&sma);
    led_state_t led = LED_OFF;
    gpio_set_level(GPIO_NUM_16, 0);

    while (1) {
    int raw;
    adc_oneshot_read(adc, ADC_CHANNEL_3, &raw);
    uint16_t avg = sma_update(&sma, (uint16_t)raw);

    led_state_t new_led = hysteresis_update(led, avg);
    if (new_led != led) {
        led = new_led;
        gpio_set_level(GPIO_NUM_16, led);
        printf("LED -> %s (avg=%u)\n", led ? "ON" : "OFF", avg);
    }

    printf("%d,%u,%d\n", raw, avg, led);
    vTaskDelay(pdMS_TO_TICKS(50));
    }
}