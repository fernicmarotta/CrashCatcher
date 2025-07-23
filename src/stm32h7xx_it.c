/**
 * @file stm32h7xx_it.c
 * @brief Interrupt handlers
 */

#include "stm32h7xx_hal.h"

/**
 * @brief This function handles System tick timer.
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
}