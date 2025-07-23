# PE Hardfault Integration with Core Dump

## Overview

This document explains how to integrate the existing PE hardfault handler with the TCP core dump functionality.

## Integration Steps

### 1. Application Side Changes

In your application that uses `pe_hardfault`:

#### Option A: Minimal Changes (Recommended)

1. Add to your project defines:
```c
#define PE_HARDFAULT_USE_COREDUMP
```

2. Include the integration header in your `pe_hardfault.c`:
```c
#include "pe_hardfault_integration.h"
```

3. The `system_reset()` calls will automatically be replaced with `pe_hardfault_trigger_coredump()`

#### Option B: Direct Modification

Replace the `system_reset()` call in `hardfault_type_handler()` with:
```c
/* Save crash info and trigger core dump */
pe_hardfault_trigger_coredump();
```

### 2. Link the Integration Code

Add to your application build:
- `src/app/pe_hardfault_coredump.c`
- Include path: `include/`

### 3. Bootloader Configuration

The bootloader will automatically:
1. Check for crash marker in backup SRAM on startup
2. If crash detected, send complete RAM dump via TCP
3. Clear crash marker and jump to application

## How It Works

1. **Crash Occurs**: Your existing PE hardfault handler captures all register and fault information
2. **Save to Backup SRAM**: The integration code saves crash info to battery-backed SRAM
3. **System Reset**: The system resets (backup SRAM preserves data)
4. **Bootloader Detects**: On startup, bootloader checks for crash marker
5. **TCP Dump**: Bootloader sends complete RAM contents via TCP
6. **Normal Boot**: After dump, bootloader jumps to application

## Data Flow

```
Application Crash
    ↓
PE Hardfault Handler (captures registers)
    ↓
pe_hardfault_trigger_coredump() 
    ↓
Save to Backup SRAM (0x38800000)
    ↓
NVIC_SystemReset()
    ↓
Bootloader Startup
    ↓
Check Crash Marker
    ↓
Send TCP Dump (if crash detected)
    ↓
Jump to Application
```

## Memory Layout

- **Backup SRAM**: 0x38800000 - 0x38801000 (4KB)
  - Used to store crash info across reset
  - Battery-backed, survives reset
  
## Configuration

In `bootloader_config.h`:
```c
.network = {
    .device_ip = {192, 168, 1, 200},      // Bootloader IP
    .server_ip = {192, 168, 1, 10},       // Dump server IP
    .server_port = 9999,                  // Server port
}
```

## Testing

1. Trigger a hardfault in your application
2. System should reset automatically
3. Bootloader LEDs will blink during dump transmission
4. TCP server should receive the dump

## Advantages Over CrashCatcher HexDump

1. **No UART needed**: Dumps sent via Ethernet
2. **Complete RAM dump**: ~1MB vs limited HexDump
3. **Faster**: TCP transmission vs slow UART
4. **Non-intrusive**: Dump happens after reset with clean system
5. **Preserves PE hardfault features**: All your existing diagnostics still work