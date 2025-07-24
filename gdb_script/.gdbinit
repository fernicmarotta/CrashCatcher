# ======================================
# GDB Init for STM32H743 + Core Dumps
# ======================================

# Basic ARM configuration
set print pretty on
set print object on
set print static-members on
set print vtbl on
set print demangle on
set demangle-style gnu-v3
set print array on

# For STM32H7
set architecture armv7e-m
set arm fallback-mode thumb
set arm force-mode thumb

# ======================================
# Function to load crash dump (ELF core file)
# ======================================
define load_crash
  if $argc == 0
    echo Usage: load_crash <path_to_core_dump.elf>\n
  else
    echo \n=== Loading STM32H743 crash dump (ELF format) ===\n
    echo File: $arg0\n\n

    # Load the ELF core file
    echo Loading core file...\n
    core $arg0

    echo \n=== State restored! ===\n
    printf "PC = 0x%08x  ", $pc
    x/i $pc
    printf "LR = 0x%08x  ", $lr
    x/i $lr
    printf "SP = 0x%08x\n", $sp

    # Show all registers
    echo \n=== CPU Registers ===\n
    info registers

    # Analyze the crash
    analyze_crash
  end
end

# ======================================
# Function to load crash dump (OLD BINARY FORMAT)
# ======================================
define load_crash_binary
  if $argc == 0
    echo Usage: load_crash_binary <path_to_dump.bin>\n
  else
    echo \n=== Loading STM32H743 crash dump (binary format) ===\n
    echo File: $arg0\n\n

    # Syntax: restore <file> binary <bias> <start_in_file> <end_in_file>

    echo [1/6] RAM D1 (AXI SRAM @ 0x24000000)...\n
    restore $arg0 binary 0x24000000 0x0001bc 0x801bc

    echo [2/6] RAM D2 (SRAM1 @ 0x30000000)...\n
    restore $arg0 binary 0x30000000 0x0801bc 0xa01bc

    echo [3/6] RAM D2 (SRAM2 @ 0x30020000)...\n
    restore $arg0 binary 0x30020000 0x0a01bc 0xc01bc

    echo [4/6] RAM D3 (SRAM4/Ethernet @ 0x30040000)...\n
    restore $arg0 binary 0x30040000 0x0c01bc 0xc81bc

    echo [5/6] RAM D3 (SRAM3 @ 0x38000000)...\n
    restore $arg0 binary 0x38000000 0x0c81bc 0xd81bc

    echo [6/6] Backup SRAM (@ 0x38800000)...\n
    restore $arg0 binary 0x38800000 0x0d81bc 0xd91bc

    # Load registers
    echo \n=== Restoring registers ===\n
    restore $arg0 binary 0x20000000 0x114 0x1a8

    set $r0  = *(unsigned int*)(0x20000000 + 0x18)
    set $r1  = *(unsigned int*)(0x20000000 + 0x1C)
    set $r2  = *(unsigned int*)(0x20000000 + 0x20)
    set $r3  = *(unsigned int*)(0x20000000 + 0x24)
    set $r4  = *(unsigned int*)(0x20000000 + 0x28)
    set $r5  = *(unsigned int*)(0x20000000 + 0x2C)
    set $r6  = *(unsigned int*)(0x20000000 + 0x30)
    set $r7  = *(unsigned int*)(0x20000000 + 0x34)
    set $r8  = *(unsigned int*)(0x20000000 + 0x38)
    set $r9  = *(unsigned int*)(0x20000000 + 0x3C)
    set $r10 = *(unsigned int*)(0x20000000 + 0x40)
    set $r11 = *(unsigned int*)(0x20000000 + 0x44)
    set $r12 = *(unsigned int*)(0x20000000 + 0x48)
    set $sp  = *(unsigned int*)(0x20000000 + 0x4C)
    set $lr  = *(unsigned int*)(0x20000000 + 0x50)
    set $pc  = *(unsigned int*)(0x20000000 + 0x54)
    set $xpsr = *(unsigned int*)(0x20000000 + 0x58)

    echo \n=== State restored! ===\n
    printf "PC = 0x%08x  ", $pc
    x/i $pc
    printf "LR = 0x%08x  ", $lr
    x/i $lr
    printf "SP = 0x%08x\n", $sp

    # Analyze the crash
    analyze_crash
  end
end

# ======================================
# Analyze crash cause
# ======================================
define analyze_crash
  echo \n=== Crash analysis ===\n

  # Read Fault Status Registers
  set $cfsr = *(unsigned int*)0xE000ED28
  set $hfsr = *(unsigned int*)0xE000ED2C
  set $dfsr = *(unsigned int*)0xE000ED30
  set $mmfar = *(unsigned int*)0xE000ED34
  set $bfar = *(unsigned int*)0xE000ED38

  printf "CFSR  = 0x%08x\n", $cfsr
  printf "HFSR  = 0x%08x\n", $hfsr
  printf "DFSR  = 0x%08x\n", $dfsr

  # Analyze CFSR
  if ($cfsr & 0x00000001)
    echo  - IACCVIOL: Instruction access violation\n
  end
  if ($cfsr & 0x00000002)
    echo  - DACCVIOL: Data access violation\n
  end
  if ($cfsr & 0x00000008)
    echo  - MUNSTKERR: MemManage fault on exception unstacking\n
  end
  if ($cfsr & 0x00000010)
    echo  - MSTKERR: MemManage fault on exception stacking\n
  end
  if ($cfsr & 0x00000020)
    echo  - MLSPERR: MemManage fault on lazy state preservation\n
  end
  if ($cfsr & 0x00000080)
    printf " - MMFAR valid: 0x%08x\n", $mmfar
  end

  # Bus Faults
  if ($cfsr & 0x00000100)
    echo  - IBUSERR: Bus fault on instruction fetch\n
  end
  if ($cfsr & 0x00000200)
    echo  - PRECISERR: Precise data bus error\n
  end
  if ($cfsr & 0x00000400)
    echo  - IMPRECISERR: Imprecise data bus error\n
  end
  if ($cfsr & 0x00000800)
    echo  - UNSTKERR: Bus fault on unstacking\n
  end
  if ($cfsr & 0x00001000)
    echo  - STKERR: Bus fault on stacking\n
  end
  if ($cfsr & 0x00002000)
    echo  - LSPERR: Bus fault on lazy state preservation\n
  end
  if ($cfsr & 0x00008000)
    printf " - BFAR valid: 0x%08x\n", $bfar
  end

  # Usage Faults
  if ($cfsr & 0x00010000)
    echo  - UNDEFINSTR: Undefined instruction\n
  end
  if ($cfsr & 0x00020000)
    echo  - INVSTATE: Invalid EPSR.T state\n
  end
  if ($cfsr & 0x00040000)
    echo  - INVPC: Invalid PC value\n
  end
  if ($cfsr & 0x00080000)
    echo  - NOCP: Coprocessor not enabled\n
  end
  if ($cfsr & 0x00100000)
    echo  - STKOF: Stack overflow (if enabled)\n
  end
  if ($cfsr & 0x01000000)
    echo  - DIVBYZERO: Division by zero\n
  end
  if ($cfsr & 0x02000000)
    echo  - UNALIGNED: Unaligned access\n
  end

  # Hard Fault
  if ($hfsr & 0x00000002)
    echo  - VECTTBL: Vector table fault\n
  end
  if ($hfsr & 0x40000000)
    echo  - FORCED: Hard fault forced by another fault\n
  end
  if ($hfsr & 0x80000000)
    echo  - DEBUGEVT: Debug event\n
  end

  echo \nBacktrace:\n
  bt
end

# ======================================
# Useful helpers
# ======================================
define dump_stack
  echo \n=== Stack dump ===\n
  x/32xw $sp
end

define show_heap
  echo \n=== Heap info ===\n
  # Adjust according to your allocator
  p _sbrk_r
  p __malloc_free_list
end

define reset_target
  echo Resetting target...\n
  monitor reset halt
end

# ======================================
# Short aliases
# ======================================
alias lc = load_crash
alias ac = analyze_crash
alias ds = dump_stack
alias rt = reset_target

# ======================================
# Auto-configuration on connect
# ======================================
define hook-target-remote
  echo \n=== Connected to STM32H743 ===\n
  echo Available commands:\n
  echo   load_crash <file.elf>     - Load ELF core dump\n
  echo   load_crash_binary <file>  - Load binary dump (old format)\n
  echo   analyze_crash             - Show crash cause\n
  echo   dump_stack                - Stack dump\n
  echo   reset_target              - Reset MCU\n
  echo \nAliases: lc, ac, ds, rt\n\n
end

# ======================================
# Auto-load last crash
# ======================================
# Uncomment and adjust path if you want automatic loading
# define hook-stop
#   if $pc == main
#     load_crash /home/fnicolas/Documentos/GIT/pe-core-dump/tools/core_20250724_190540.elf
#   end
# end

echo .gdbinit loaded - STM32H743 ready!\n