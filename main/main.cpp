
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include <stdio.h>

static void app_task(void *arg)
{
    while (true) {
        
    }
}

extern "C" void app_main(void)
{
    xTaskCreate(app_task,     "app",     4096, NULL, 5, NULL);
}