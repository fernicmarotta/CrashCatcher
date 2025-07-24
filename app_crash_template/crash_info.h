/**
 * @file crash_info.h
 * @brief Crash information structure stored in Backup SRAM
 * 
 * This structure is shared between the application and bootloader.
 * It's stored at the beginning of Backup SRAM (0x38800000).
 */

#ifndef CRASH_INFO_H
#define CRASH_INFO_H

#include <stdint.h>

/* Backup SRAM base address on STM32H7 */
#define BACKUP_SRAM_BASE    0x38800000UL

/* Magic values */
#define CRASH_MAGIC_VALID   0xDEADC0DEUL
#define CRASH_MAGIC_CLEAR   0x00000000UL

/* Crash types */
typedef enum {
    CRASH_TYPE_NONE = 0,
    CRASH_TYPE_HARDFAULT,
    CRASH_TYPE_MEMMANAGE,
    CRASH_TYPE_BUSFAULT,
    CRASH_TYPE_USAGEFAULT,
    CRASH_TYPE_ASSERT,
    CRASH_TYPE_WATCHDOG,
    CRASH_TYPE_STACKOVF
} crash_type_t;

/* Crash information structure - fits in first 256 bytes of Backup SRAM */
typedef struct {
    /* Header */
    uint32_t magic;             /* 0x00: Magic number (0xDEADC0DE) */
    uint32_t crash_type;        /* 0x04: Type of crash */
    uint32_t timestamp;         /* 0x08: System tick when crashed */
    uint32_t boot_count;        /* 0x0C: Number of boots since last clear */
    
    /* Core registers at time of fault */
    uint32_t r0;                /* 0x10 */
    uint32_t r1;                /* 0x14 */
    uint32_t r2;                /* 0x18 */
    uint32_t r3;                /* 0x1C */
    uint32_t r4;                /* 0x20 */
    uint32_t r5;                /* 0x24 */
    uint32_t r6;                /* 0x28 */
    uint32_t r7;                /* 0x2C */
    uint32_t r8;                /* 0x30 */
    uint32_t r9;                /* 0x34 */
    uint32_t r10;               /* 0x38 */
    uint32_t r11;               /* 0x3C */
    uint32_t r12;               /* 0x40 */
    uint32_t sp;                /* 0x44: Stack pointer (MSP or PSP) */
    uint32_t lr;                /* 0x48: Link register */
    uint32_t pc;                /* 0x4C: Program counter (where it crashed) */
    uint32_t psr;               /* 0x50: Program status register */
    
    /* Stack pointers */
    uint32_t msp;               /* 0x54: Main stack pointer */
    uint32_t psp;               /* 0x58: Process stack pointer */
    
    /* Fault status registers */
    uint32_t cfsr;              /* 0x5C: Configurable Fault Status Register */
    uint32_t hfsr;              /* 0x60: HardFault Status Register */
    uint32_t dfsr;              /* 0x64: Debug Fault Status Register */
    uint32_t mmfar;             /* 0x68: MemManage Fault Address Register */
    uint32_t bfar;              /* 0x6C: BusFault Address Register */
    uint32_t afsr;              /* 0x70: Auxiliary Fault Status Register */
    
    /* Additional info */
    uint32_t lr_at_fault;       /* 0x74: LR value at exception entry */
    uint32_t app_version;       /* 0x78: Application version/build */
    uint32_t reserved[2];       /* 0x7C: Reserved for future use */
    
    /* Checksum */
    uint32_t checksum;          /* 0x84: CRC32 of all above fields */
    
} crash_info_t;

/* Ensure structure is exactly the size we expect */
_Static_assert(sizeof(crash_info_t) == 0x88, "crash_info_t size mismatch");

/* Pointer to crash info in Backup SRAM */
#define CRASH_INFO          ((volatile crash_info_t*)BACKUP_SRAM_BASE)

/* Helper macros */
#define CRASH_INFO_IS_VALID()   (CRASH_INFO->magic == CRASH_MAGIC_VALID)
#define CRASH_INFO_CLEAR()      do { CRASH_INFO->magic = CRASH_MAGIC_CLEAR; } while(0)

/* Function prototypes */
#ifdef __cplusplus
extern "C" {
#endif

/* Calculate CRC32 for crash info (implement in your app) */
uint32_t crash_info_calc_crc(const crash_info_t* info);

/* Validate crash info checksum */
int crash_info_is_valid(const crash_info_t* info);

/* Clear crash info */
void crash_info_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* CRASH_INFO_H */