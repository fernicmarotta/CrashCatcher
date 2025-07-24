/**
 * @file crash_dump.h
 * @brief Crash dump public interface
 */

#ifndef __CRASH_DUMP_H
#define __CRASH_DUMP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "crash_info.h"

/**
 * @brief Check if there's a valid crash dump in backup SRAM
 * @return 1 if valid crash dump exists, 0 otherwise
 */
int crash_dump_is_valid(void);

/**
 * @brief Clear crash dump from backup SRAM
 */
void crash_dump_clear(void);

/**
 * @brief Get crash info (if valid)
 * @param info Pointer to structure to fill
 * @return 1 if valid crash info was copied, 0 otherwise
 */
int crash_dump_get_info(crash_info_t* info);

/**
 * @brief Force a test crash (HardFault)
 * WARNING: This will crash the system!
 */
void crash_dump_test(void);

#ifdef __cplusplus
}
#endif

#endif /* __CRASH_DUMP_H */