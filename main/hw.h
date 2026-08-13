#pragma once

#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include <stdint.h>

// --- Лабораторна: дребезг контактів ---------------------------------------
// Кнопка: GPIO4 -> GND
//   без RC: внутрішня підтяжка увімкнена
//   з RC:   зовнішні 10 кОм + 100 Ом + 100 нФ, внутрішня вимкнена
// Світлодіод: GPIO2 + резистор -> GND. Перемикається на кожну прийняту подію.

constexpr gpio_num_t PIN_BTN = GPIO_NUM_4;
constexpr gpio_num_t PIN_LED = GPIO_NUM_2;

// Кнопка на землю -> натиснута дає LOW.
constexpr int BTN_PRESSED = 0;

// Спільний для всіх режимів стан внутрішньої підтяжки. Змінюється в рантаймі
// (командою з UART), тому не constexpr. Щоб нове значення застосувалося до
// заліза, режим має перевикликати gpio_config -> exit() + enter().
extern gpio_pullup_t g_pullup;

void hwSetPullup(bool enabled);
bool hwPullupEnabled();

static inline uint32_t millis() {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

// --- Спільне джерело фронтів ----------------------------------------------
// Три режими з чотирьох налаштовують пін однаково: переривання по спаду,
// обробник кладе мітку часу в чергу. Різниця між ними — виключно в тому, як
// вони цю чергу читають, тож механіка винесена сюди.
//
// edgeSourceStart() створює чергу, конфігурує пін і вішає обробник.
// Повертає nullptr, якщо щось не вдалося — режим має це перевірити.
// edgeSourceStop() знімає обробник і видаляє чергу (саме в такому порядку).

QueueHandle_t edgeSourceStart();
void          edgeSourceStop(QueueHandle_t &q);

// Налаштовує пін на читання без переривань — для режиму polling.
esp_err_t hwConfigButtonPolling();