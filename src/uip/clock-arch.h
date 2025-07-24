#ifndef CLOCK_ARCH_H
#define CLOCK_ARCH_H

/* Clock architecture for STM32H7 using HAL_GetTick() */
#include "stm32h7xx_hal.h"

typedef uint32_t clock_time_t;

#define CLOCK_CONF_SECOND 1000  /* 1000 ticks per second (ms) */

/* Get current clock time - use function instead of macro */
static inline clock_time_t clock_time(void) {
    return HAL_GetTick();
}

#endif /* CLOCK_ARCH_H */