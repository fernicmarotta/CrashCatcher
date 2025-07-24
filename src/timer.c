#include "timer.h"
#include "stm32h7xx_hal.h"

void timer_set(struct timer *t, uint32_t interval) {
    t->interval = interval;
    t->start = HAL_GetTick();
}

void timer_reset(struct timer *t) {
    t->start = HAL_GetTick();
}

int timer_expired(struct timer *t) {
    return (HAL_GetTick() - t->start) >= t->interval;
}