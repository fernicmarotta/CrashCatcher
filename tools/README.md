# STM32H7 Crash Dump Tools

## Overview

These tools receive crash dumps from STM32H7 devices over TCP and create GDB-compatible core dump files for post-mortem debugging.

## crash_dump_server.py

TCP server that:
- Listens for crash dumps in hexdump format
- Parses CPU registers and memory contents
- Creates ELF core dump files compatible with GDB
- Displays crash analysis summary

### Usage

```bash
# Start server on default port 9999
./crash_dump_server.py

# Or specify custom host/port
./crash_dump_server.py --host 192.168.1.100 --port 8888
```

### Output

When a crash dump is received, the server will:

1. **Display crash summary**:
   - All CPU registers (R0-R12, SP, LR, PC, PSR)
   - Fault status registers (HFSR, CFSR, MMFAR, BFAR, AFSR)
   - Decoded fault reasons
   - Memory regions received with sizes

2. **Create core dump file**:
   - Filename: `core_YYYYMMDD_HHMMSS.elf`
   - Compatible with arm-none-eabi-gdb
   - Contains all registers and memory contents

### Debugging with GDB

```bash
# 1. Copy your application ELF
cp /path/to/your/app.elf .

# 2. Start GDB with your application
arm-none-eabi-gdb app.elf

# 3. Load the core dump
(gdb) target core core_20240724_143022.elf

# 4. Analyze the crash
(gdb) bt                    # Show backtrace
(gdb) info registers        # Show all registers
(gdb) list                  # Show source code at crash location
(gdb) x/10x $sp            # Examine stack
(gdb) disas $pc-16,$pc+16  # Show disassembly around crash
```

## test_crash_client.py

Test client that simulates a crash dump for testing the server.

```bash
# Make sure server is running first, then:
./test_crash_client.py
```

## Protocol Format

The bootloader sends crash dumps in text format:

```
\r\n\r\nCRASH ENCOUNTERED\r\n
R0: XXXXXXXX
R1: XXXXXXXX
...
PC: XXXXXXXX
PSR: XXXXXXXX
HFSR: XXXXXXXX
...
\r\nMemory dump:\r\n
\r\nDTCM:\r\n
20000000: XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX
20000010: XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX
...
\r\nAXI_SRAM:\r\n
24000000: XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX
...
\r\nEnd of dump\r\n
```

## Memory Regions

The STM32H7 memory regions dumped:
- **DTCM**: 0x20000000 - 0x20020000 (128KB)
- **AXI_SRAM**: 0x24000000 - 0x24080000 (512KB)  
- **SRAM1**: 0x30000000 - 0x30020000 (128KB)
- **SRAM2**: 0x30020000 - 0x30040000 (128KB)
- **SRAM3**: 0x30040000 - 0x30048000 (32KB)
- **SRAM4**: 0x38000000 - 0x38010000 (64KB)
- **BACKUP_SRAM**: 0x38800000 - 0x38801000 (4KB)

Total: ~1MB of RAM content

## Requirements

- Python 3.6+
- No external dependencies (uses only standard library)

## Notes

- The server creates ARM core dumps (ET_CORE) with NT_PRSTATUS notes
- Compatible with standard GDB for Cortex-M targets
- Memory is saved exactly as received, preserving crash state
- Works with any GDB-aware IDE (VSCode, Eclipse, etc.)