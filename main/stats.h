#pragma once
#include <stdint.h>
#include "esp_attr.h"

// Спільні лічильники для всіх режимів. Один набір на серію вимірювань.

void statsReset();
void statsSetExpected(uint16_t n);   // скільки натискань у серії (типово 10)
void statsSetRC(bool installed);     // чи є RC-фільтр

void IRAM_ATTR statsRawEdge();       // сирий фронт (тільки там, де є ISR)
void statsReaction();                // прийнята подія = одна реакція системи

uint32_t statsReactions();
uint32_t statsRawEdges();

void statsPrintHeader();
void statsPrintRow(const char* mode_name);
