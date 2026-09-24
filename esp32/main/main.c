#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define LINK_UART    UART_NUM_1
#define LINK_TX_PIN  17
#define LINK_RX_PIN  18
#define LINK_BAUD    9600          // має збігатися з USART2 на STM32
#define RX_BUF_SIZE  256

#define LED_PIN      GPIO_NUM_4

static const char *TAG = "LINK";

static void link_init(void)
{
    uart_config_t cfg = {
        .baud_rate  = LINK_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(LINK_UART, RX_BUF_SIZE, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(LINK_UART, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(LINK_UART, LINK_TX_PIN, LINK_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    gpio_set_pull_mode(LINK_RX_PIN, GPIO_PULLUP_ONLY);
}

static void link_send(const char *s)
{
    uart_write_bytes(LINK_UART, s, strlen(s));
}

static bool read_line(char *buf, size_t len)
{
    size_t n = 0;
    while (1) {
        uint8_t c;
        if (uart_read_bytes(LINK_UART, &c, 1, portMAX_DELAY) != 1) continue;
        if (c == '\r') continue;
        if (c == '\n') { buf[n] = '\0'; return true; }
        if (n < len - 1) buf[n++] = (char)c;
        else n = 0;                          // занадто довгий рядок — скидаємо
    }
}

static void handle_command(const char *cmd)
{
    if (strcmp(cmd, "ON") == 0) {
        gpio_set_level(LED_PIN, 1);
        link_send("ACK ON\n");
        ESP_LOGI(TAG, "RX 'ON'  -> LED ON");
    } else if (strcmp(cmd, "OFF") == 0) {
        gpio_set_level(LED_PIN, 0);
        link_send("ACK OFF\n");
        ESP_LOGI(TAG, "RX 'OFF' -> LED OFF");
    } else if (cmd[0] != '\0') {
        link_send("ERR\n");
        ESP_LOGW(TAG, "unknown cmd: '%s'", cmd);
    }
}

void app_main(void)
{
    link_init();

    gpio_config_t led = {
        .pin_bit_mask = 1ULL << LED_PIN,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&led));
    gpio_set_level(LED_PIN, 0);

    ESP_LOGI(TAG, "Start. UART1 %d 8N1, TX=%d RX=%d, LED GPIO%d",
             LINK_BAUD, LINK_TX_PIN, LINK_RX_PIN, LED_PIN);

    char line[32];
    while (1) {
        if (read_line(line, sizeof line)) {
            handle_command(line);
        }
    }
}