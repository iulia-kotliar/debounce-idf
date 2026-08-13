#include "hw.h"

gpio_pullup_t g_pullup = GPIO_PULLUP_ENABLE;

void hwSetPullup(bool enabled) {
    g_pullup = enabled ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
}

bool hwPullupEnabled() {
    return g_pullup == GPIO_PULLUP_ENABLE;
}