#include "hw.h"
#include "mode.h"
#include "stats.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include <stdio.h>

static const char *TAG = "main";

static const Mode* const modes[] = { &mode_none, &mode_time, &mode_state, &mode_poll };
static constexpr uint8_t MODE_COUNT = sizeof(modes) / sizeof(modes[0]);

static uint8_t current = 0;

// Консоль і робочий цикл живуть в окремих задачах: читання stdin блокує,
// і без розділення воно б зупиняло опитування кнопки. Команди передаються
// через чергу, тож exit()/enter() виконуються лише в робочій задачі й
// ніколи не перетинаються з tick().
static QueueHandle_t cmd_q = nullptr;

struct Cmd {
    char code;      // '1'..'4', 'r', 'p', 'c', 'n', 'h'
    uint16_t arg;   // для 'n'
};

static void printHelp() {
    printf("\n");
    printf("1..4 — режим (1 без debounce, 2 time, 3 state, 4 polling)\n");
    printf("r    — обнулити лічильники й почати нову серію\n");
    printf("p    — надрукувати рядок таблиці\n");
    printf("c    — перемкнути внутрішню підтяжку (вимкнути при RC-фільтрі)\n");
    printf("n<N> — скільки натискань у серії (типово 10), напр. n20\n");
    printf("h    — ця довідка\n");
}

static void switchMode(uint8_t idx) {
    if (idx >= MODE_COUNT) return;
    modes[current]->exit();
    current = idx;
    statsReset();
    modes[current]->enter();
    printf("\n>>> режим: %s | лічильники обнулено\n", modes[current]->name);
}

// Підтяжка читається лише всередині enter(), тому після зміни прапорця
// режим треба перезапустити — інакше нова конфігурація не потрапить у залізо.
static void togglePullup() {
    hwSetPullup(!hwPullupEnabled());
    statsSetRC(!hwPullupEnabled());

    modes[current]->exit();
    modes[current]->enter();

    printf("\n>>> внутрішня підтяжка: %s | RC-фільтр: %s\n",
           hwPullupEnabled() ? "увімкнена" : "вимкнена",
           hwPullupEnabled() ? "немає"     : "припаяний");
}

static void handleCmd(const Cmd &cmd) {
    if (cmd.code >= '1' && cmd.code <= '0' + MODE_COUNT) {
        switchMode(cmd.code - '1');
        return;
    }

    switch (cmd.code) {
        case 'r':
            statsReset();
            printf(">>> лічильники обнулено\n");
            break;
        case 'p':
            statsPrintRow(modes[current]->name);
            break;
        case 'c':
            togglePullup();
            break;
        case 'n':
            if (cmd.arg > 0) {
                statsSetExpected(cmd.arg);
                printf(">>> натискань у серії: %u\n", (unsigned)cmd.arg);
            }
            break;
        case 'h':
            printHelp();
            break;
        default:
            break;
    }
}

// Задача консолі: блокується на читанні stdin і нічого не робить між
// командами. Розбір числа для 'n' робиться тут, щоб робоча задача
// отримувала вже готову команду.
static void console_task(void *arg)
{
    while (true) {
        int c = fgetc(stdin);

        if (c == EOF) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        Cmd cmd = { (char)c, 0 };

        if (c == 'n') {
            uint16_t n = 0;
            int d;
            while ((d = fgetc(stdin)) >= '0' && d <= '9') {
                n = n * 10 + (uint16_t)(d - '0');
            }
            cmd.arg = n;
        }

        xQueueSend(cmd_q, &cmd, 0);
    }
}

// Робоча задача: крутить tick() поточного режиму й обробляє команди.
// vTaskDelay(1) віддає процесор системі; при частоті тика 1000 Гц це 1 мс,
// що дрібніше за POLL_MS = 5 у режимі polling.
static void app_task(void *arg)
{
    modes[current]->enter();
    printf("\n>>> режим: %s\n", modes[current]->name);

    while (true) {
        Cmd cmd;
        while (xQueueReceive(cmd_q, &cmd, 0) == pdTRUE) {
            handleCmd(cmd);
        }

        modes[current]->tick();
        vTaskDelay(1);
    }
}

extern "C" void app_main(void)
{
    // stdout без буферизації: інакше рядки таблиці зависають у буфері
    // й з'являються в моніторі пачками з затримкою.
    setvbuf(stdout, NULL, _IONBF, 0);

    gpio_reset_pin(PIN_LED);
    gpio_set_direction(PIN_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_LED, 0);

    // Служба переривань ставиться один раз на весь застосунок;
    // режими лише додають і знімають власні обробники.
    gpio_install_isr_service(0);

    cmd_q = xQueueCreate(8, sizeof(Cmd));
    if (!cmd_q) {
        ESP_LOGE(TAG, "не вдалося створити чергу команд");
        return;
    }

    printf("\n=== Дребезг контактів: порівняння методів ===\n");
    printHelp();
    statsPrintHeader();
    statsReset();

    xTaskCreate(app_task,     "app",     4096, NULL, 5, NULL);
    xTaskCreate(console_task, "console", 4096, NULL, 4, NULL);
}