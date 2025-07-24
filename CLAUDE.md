# pe-core-dump Project Documentation

This document provides comprehensive information about the pe-core-dump project for future Claude instances.

## Project Overview

pe-core-dump is a STM32H743-based crash dump system that integrates:
- **CrashCatcher**: ARM Cortex-M crash dump library
- **lwIP 2.3.0**: Lightweight TCP/IP stack (NO_SYS mode)
- **TCP Dump**: Transmits crash dumps via Ethernet instead of HexDump
- **Bootloader Architecture**: Preserves RAM and sends dumps after reset

### Key Features
- Captures complete system state on HardFault/exceptions
- Sends ~1MB RAM dump via TCP using minimal resources
- Uses only ITCMRAM (64KB) during normal operation
- Bootloader preserves RAM content after crash
- Optimized binary size: <65KB flash footprint

## Build System

### Prerequisites
- pe-ccpp-tools (custom build system)
- ARM toolchain (arm-none-eabi-gcc)
- CMake 3.20+
- Wine (for Windows tools under Linux)

### Quick Build
```bash
# Build everything
pe-ccpp-tool build

# IMPORTANT: pe-ccpp-tool clean DOES NOT EXIST!
# To clean, use:
rm -rf build
pe-ccpp-tool build

# Flash application
pe-ccpp-tool flash

# Build specific target
cd build
cmake ..
make CORTEX_COREDUMP
```

## Directory Structure

```
pe-core-dump/
├── bootloader/             # Bootloader that sends dumps after reset
│   ├── src/               # Bootloader sources
│   ├── inc/               # Bootloader headers
│   └── bootloader.ld      # Linker script (uses only ITCM)
├── cmake/                  # CMake configuration files
│   ├── cflags.cmake       # -Os optimization, no cache
│   ├── defines.cmake      # Core defines
│   ├── includes.cmake     # Include paths
│   ├── libraries.cmake    # HAL libraries
│   ├── lwip.cmake         # lwIP integration
│   └── subdirectories.cmake
├── Flash/
│   └── CORTEX_COREDUMP.ld # App linker script (starts at 0x08010000)
├── include/
│   ├── crash_info.h       # Shared crash info structure
│   └── tcp_crash_dump.h   # TCP dump interface
├── src/
│   ├── app/               # Application code
│   │   └── minimal_fault_handler.s  # Saves info and resets
│   ├── CrashCatcher/      # Modified CrashCatcher
│   ├── lwip/              # lwIP configuration
│   │   └── lwipopts.h     # Minimal config
│   └── TcpDump/           # TCP crash dump implementation
├── .gitignore             # Ignore HAL/BSP files
├── CLAUDE.md              # This file
└── CMakeLists.txt         # Main CMake file
```

## Architecture Details

### Bootloader Design

The system uses a bootloader architecture to handle crashes safely:

#### Memory Layout
- **0x08000000 - 0x0800FFFF**: Bootloader (64KB)
- **0x08010000 - 0x081FFFFF**: Application (1984KB)

#### Operation Flow
1. **Normal Operation**: App runs from 0x08010000
2. **Crash Occurs**: 
   - Minimal fault handler saves critical info to Backup SRAM
   - Immediate system reset
3. **Bootloader Detects Crash**:
   - Checks magic marker in Backup SRAM (0x38800000)
   - Initializes network (using only ITCM)
   - Sends complete RAM dump via TCP
   - Clears crash marker
4. **Jump to Application**

#### IMPORTANT: How This Works
- **pe-core-dump IS the bootloader** that RECEIVES crashes from OTHER applications
- **Other applications** (like nb_combiner) implement their crash handler using app_crash_template
- **app_crash_template/** shows how applications should save crash info to Backup SRAM
- The bootloader reads this info after reset and sends it via TCP

### Memory Configuration

The system is configured to use ONLY ITCMRAM (64KB) with caches disabled:
- ITCMRAM: 0x00000000 - 0x00010000 (64KB)
- No other RAM regions used during normal operation
- I-Cache and D-Cache are DISABLED to prevent corruption

### RAM Regions Dumped

When a crash occurs, the bootloader sends ALL RAM except ITCM:
- DTCM RAM: 0x20000000 - 0x20020000 (128KB)
- AXI SRAM: 0x24000000 - 0x24080000 (512KB)
- SRAM1: 0x30000000 - 0x30020000 (128KB)
- SRAM2: 0x30020000 - 0x30040000 (128KB)
- SRAM3: 0x30040000 - 0x30048000 (32KB)
- SRAM4: 0x38000000 - 0x38010000 (64KB)
- Backup SRAM: 0x38800000 - 0x38801000 (4KB)

**Total**: ~1MB of data transmitted

### Crash Information Structure

Located at 0x38800000 (Backup SRAM), contains:
- Magic number (0xDEADC0DE)
- Crash type (HardFault, MemManage, etc.)
- CPU registers (r0-r12, sp, lr, pc, psr)
- Fault registers (CFSR, HFSR, MMFAR, BFAR)
- CRC32 checksum

## Building and Flashing

### Build Bootloader
```bash
cd build
cmake ..
make pe-core-dump-bootloader.elf
make flash-bootloader  # Flash to 0x08000000
```

### Build Application
```bash
pe-ccpp-tool build
# or
make CORTEX_COREDUMP
```

### Flash Both Components
```bash
# Flash bootloader first (only once)
make flash-bootloader

# Flash application
pe-ccpp-tool flash
# or manually:
st-flash write CORTEX_COREDUMP.bin 0x08020000
```

## Network Configuration

### Device Settings
- Bootloader IP(nosotrols): 192.168.204.16
- Dump Server: 192.168.204.134:9999

### TCP Dump Protocol
1. Connection to server on port 9999
2. Header with crash info
3. Binary memory dump
4. Connection close

## lwIP Configuration

Key settings in `lwipopts.h`:
```c
#define NO_SYS                          1
#define MEM_SIZE                        (2*1024)  // 2KB heap
#define MEMP_NUM_TCP_PCB                1
#define TCP_MSS                         256
#define LWIP_DISABLE_TCP_SANITY_CHECKS  1
```

## Important Notes

1. **Safety**: The bootloader approach ensures dump is sent with a clean system
2. **RAM Preservation**: Bootloader uses ONLY ITCM, preserving all other RAM
3. **Binary Size**: Total system <130KB (65KB bootloader + 65KB app)
4. **No DMA**: All operations are polling-based for safety
5. **Reset Safe**: Works even after power loss (Backup SRAM preserves crash info)

## Testing

1. **Trigger Test Crash**:
```c
// Force a HardFault
*((volatile uint32_t*)0) = 0xDEADBEEF;
```

2. **Run TCP Server**:
```bash
nc -l 9999 > crash_dump.bin
```

3. **Analyze Dump**: Use CrashDebug tool with the dump file

## Troubleshooting

- **No dump received**: Check network cable, server is listening
- **Bootloader timeout**: Default 30s, may need adjustment
- **Size issues**: Use `make size-bootloader` to verify <64KB

## Future Improvements

- Add DHCP support (currently static IP only)
- Compress dumps before sending
- Add encryption for sensitive dumps
- Support multiple dump destinations