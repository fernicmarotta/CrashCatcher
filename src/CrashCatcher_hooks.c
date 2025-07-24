/**
 * CrashCatcher hooks implementation
 * Minimal implementation that saves crash info to backup SRAM
 */

#include "CrashCatcher.h"
#include "crash_info.h"
#include "stm32h7xx_hal.h"
#include <string.h>

/* Crash info location in backup SRAM */
static crash_info_t* g_crash_info = (crash_info_t*)BACKUP_SRAM_BASE;

/* Current pointer for memory dumps */
static uint8_t* g_dump_ptr = NULL;

/* Memory regions to include in dump */
const CrashCatcherMemoryRegion* CrashCatcher_GetMemoryRegions(void) {
    static const CrashCatcherMemoryRegion regions[] = {
        {0x20000000, 0x20020000, CRASH_CATCHER_BYTE},  /* DTCM */
        {0x24000000, 0x24080000, CRASH_CATCHER_BYTE},  /* AXI SRAM */
        {0x30000000, 0x30020000, CRASH_CATCHER_BYTE},  /* SRAM1 */
        {0x30020000, 0x30040000, CRASH_CATCHER_BYTE},  /* SRAM2 */
        {0x30040000, 0x30048000, CRASH_CATCHER_BYTE},  /* SRAM3 */
        {0x38000000, 0x38010000, CRASH_CATCHER_BYTE},  /* SRAM4 */
        {0x38800000, 0x38801000, CRASH_CATCHER_BYTE},  /* Backup SRAM */
        {0xFFFFFFFF, 0xFFFFFFFF, CRASH_CATCHER_BYTE}   /* End marker */
    };
    return regions;
}

/* Called at start of crash dump */
void CrashCatcher_DumpStart(const CrashCatcherInfo* pInfo) {
    /* Clear crash info structure */
    memset(g_crash_info, 0, sizeof(crash_info_t));
    
    /* Set magic and crash type */
    g_crash_info->magic = CRASH_MAGIC_VALID;
    
    /* Determine crash type from flags */
    if (pInfo->isBKPT) {
        g_crash_info->crash_type = CRASH_TYPE_ASSERT;
    } else {
        g_crash_info->crash_type = CRASH_TYPE_HARDFAULT;
    }
    
    /* Save timestamp */
    g_crash_info->timestamp = HAL_GetTick();
    
    /* Point to register storage */
    g_dump_ptr = (uint8_t*)&g_crash_info->r0;
}

/* Dump memory to backup SRAM */
void CrashCatcher_DumpMemory(const void* pvMemory, CrashCatcherElementSizes elementSize, size_t elementCount) {
    size_t byteCount = elementCount;
    
    switch (elementSize) {
        case CRASH_CATCHER_BYTE:
            byteCount = elementCount;
            break;
        case CRASH_CATCHER_HALFWORD:
            byteCount = elementCount * 2;
            break;
        case CRASH_CATCHER_WORD:
            byteCount = elementCount * 4;
            break;
    }
    
    /* Only save registers and fault info (first part of crash_info_t) */
    size_t maxSize = sizeof(crash_info_t) - offsetof(crash_info_t, r0);
    size_t currentOffset = g_dump_ptr - (uint8_t*)&g_crash_info->r0;
    
    if (currentOffset < maxSize) {
        size_t toCopy = byteCount;
        if (currentOffset + toCopy > maxSize) {
            toCopy = maxSize - currentOffset;
        }
        
        memcpy(g_dump_ptr, pvMemory, toCopy);
        g_dump_ptr += toCopy;
    }
}

/* Called at end of crash dump */
CrashCatcherReturnCodes CrashCatcher_DumpEnd(void) {
    /* Reset system to let bootloader handle the dump */
    NVIC_SystemReset();
    
    /* Never reached */
    return CRASH_CATCHER_TRY_AGAIN;
}