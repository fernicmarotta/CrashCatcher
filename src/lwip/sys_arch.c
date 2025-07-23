/**
 * @file sys_arch.c
 * @brief lwIP system architecture functions for NO_SYS mode
 */

#include "lwip/opt.h"
#include "lwip/arch.h"

#if NO_SYS

/* Since we're running without an OS, most of these functions are empty */

/**
 * Initialize the sys_arch layer
 */
void sys_init(void) {
    /* Nothing to do for NO_SYS */
}

/**
 * Get current system time in milliseconds
 * This needs to be implemented based on your hardware timer
 */
u32_t sys_now(void) {
    /* TODO: Implement using STM32H7 timer */
    /* For now, return a dummy value */
    static u32_t ticks = 0;
    return ticks++;
}

/**
 * Delay for specified milliseconds
 * @param ms Milliseconds to delay
 */
void sys_msleep(u32_t ms) {
    u32_t start = sys_now();
    while ((sys_now() - start) < ms) {
        /* Busy wait - replace with proper delay if available */
        __asm("nop");
    }
}

#endif /* NO_SYS */