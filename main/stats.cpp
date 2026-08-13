#include "stats.h"
#include "hw.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include <stdio.h>

static const char *TAG = "stats";

static volatile uint32_t raw_edges = 0;
static uint32_t reactions = 0;
static uint16_t expected  = 10;
static bool     rc_on     = false;
static bool     led_state = false;
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

void statsReset() {
    portENTER_CRITICAL(&mux);
    raw_edges = 0;
    portEXIT_CRITICAL(&mux);

    reactions = 0;
    led_state = false;
    gpio_set_level(PIN_LED, 0);
    ESP_LOGI(TAG, "лічильники обнулено");
}

void statsSetExpected(uint16_t n) { expected = n; }
void statsSetRC(bool installed)   { rc_on = installed; }

void statsRawEdge() { raw_edges = raw_edges + 1; }

void statsReaction() {
    reactions++;
    led_state = !led_state;
    gpio_set_level(PIN_LED, led_state);
}

uint32_t statsReactions() { return reactions; }

uint32_t statsRawEdges() {
    portENTER_CRITICAL(&mux);
    uint32_t v = raw_edges;
    portEXIT_CRITICAL(&mux);
    return v;
}

// Таблиця друкується через printf, а не ESP_LOGI: логер додає до кожного
// рядка префікс з тегом і часом, і вирівнювання колонок розсипається.
void statsPrintHeader() {
    printf("\n");
    printf("режим          | RC  | натиснуто | реакцій | хибних | сирих фронтів\n");
    printf("---------------+-----+-----------+---------+--------+--------------\n");
}

void statsPrintRow(const char* mode_name) {
    uint32_t r   = reactions;
    uint32_t re  = statsRawEdges();
    long     bad = (long)r - (long)expected;

    // У режимі без переривань сирі фронти виміряти нічим.
    char edges[12];
    if (re == 0) snprintf(edges, sizeof(edges), "н/д");
    else         snprintf(edges, sizeof(edges), "%u", (unsigned)re);

    printf("%-14s | %-3s | %-9u | %-7u | %+-6ld | %s\n",
           mode_name,
           rc_on ? "так" : "ні",
           (unsigned)expected,
           (unsigned)r,
           bad,
           edges);
}