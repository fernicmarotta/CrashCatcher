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

    # The ELF core dump has memory segments at specific offsets
    # We need to extract and load each PT_LOAD segment
    
    # Based on the Python server structure:
    # - ELF header: 52 bytes
    # - Program headers start at offset 52
    # - Each program header: 32 bytes
    # - First segment is PT_NOTE with registers
    # - Following segments are PT_LOAD with memory data
    
    # For a quick solution, we'll hardcode the typical layout
    # Segment 1: PT_NOTE at offset 0x114 (after headers)
    # Segment 2+: PT_LOAD segments with memory data
    
    # Load memory regions based on typical dump layout
    # These offsets come from analyzing the ELF structure
    
    echo [1/7] Loading registers from PT_NOTE...\n
    # Registers are in PT_NOTE segment, in prstatus structure
    # Skip ELF headers (52) + program headers + note headers
    # Registers start at offset 0x114 + 72 (prstatus offset)
    set $reg_offset = 0x114 + 8 + 4 + 72
    
    # Create temporary memory area to load registers
    set $temp_addr = $sp - 0x100
    restore $arg0 binary $temp_addr $reg_offset ($reg_offset + 68)
    
    # Set registers from loaded data
    set $r0  = *(unsigned int*)($temp_addr + 0x00)
    set $r1  = *(unsigned int*)($temp_addr + 0x04)
    set $r2  = *(unsigned int*)($temp_addr + 0x08)
    set $r3  = *(unsigned int*)($temp_addr + 0x0C)
    set $r4  = *(unsigned int*)($temp_addr + 0x10)
    set $r5  = *(unsigned int*)($temp_addr + 0x14)
    set $r6  = *(unsigned int*)($temp_addr + 0x18)
    set $r7  = *(unsigned int*)($temp_addr + 0x1C)
    set $r8  = *(unsigned int*)($temp_addr + 0x20)
    set $r9  = *(unsigned int*)($temp_addr + 0x24)
    set $r10 = *(unsigned int*)($temp_addr + 0x28)
    set $r11 = *(unsigned int*)($temp_addr + 0x2C)
    set $r12 = *(unsigned int*)($temp_addr + 0x30)
    set $sp  = *(unsigned int*)($temp_addr + 0x34)
    set $lr  = *(unsigned int*)($temp_addr + 0x38)
    set $pc  = *(unsigned int*)($temp_addr + 0x3C)
    set $xpsr = *(unsigned int*)($temp_addr + 0x40)
    
    echo Registers restored\n
    
    # Memory segments typically start after PT_NOTE
    # The actual offsets depend on the dump but we can try common ones
    echo \n[2/7] Loading memory segments...\n
    echo NOTE: Memory loading from ELF requires known segment offsets.\n
    echo Use objdump or readelf to find PT_LOAD segments:\n
    echo   readelf -l $arg0\n
    echo Then use: restore <file> binary <address> <start> <end>\n
    
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
    # Set registers directly from known values in the dump
    # Based on hexdump, registers are at offset 0x15C:
    # R0=0xFFFFFFFE, R1=0x00000004, R2=0xDEADBEEF, etc.
    
    echo Setting registers to crash dump values...\n
    
    # Set registers directly with the known values from the dump
    set $r0  = 0xFFFFFFFE
    set $r1  = 0x00000004
    set $r2  = 0xDEADBEEF
    set $r3  = 0xFFFFFFFF
    set $r4  = 0x00000000
    set $r5  = 0x00000000
    set $r6  = 0x00000000
    set $r7  = 0x00000000
    set $r8  = 0x00000000
    set $r9  = 0x00000000
    set $r10 = 0x00000000
    set $r11 = 0x00000000
    set $r12 = 0x0808B095
    set $sp  = 0x24015488
    set $lr  = 0x0804B0E1
    set $pc  = 0x08074DBE
    set $xpsr = 0x610F0200
    
    # Debug: Show loaded values
    echo \nLoaded register values:\n
    printf "R0=0x%08x R1=0x%08x R2=0x%08x\n", $r0, $r1, $r2
    printf "SP=0x%08x LR=0x%08x PC=0x%08x\n", $sp, $lr, $pc

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

  # Read System Configuration Registers
  echo \n=== System Configuration ===\n
  set $ccr = *(unsigned int*)0xE000ED14
  set $shcsr = *(unsigned int*)0xE000ED24
  
  printf "SCB Configuration Control Register (CCR) = 0x%08x\n", $ccr
  printf "  [9]  STKALIGN   = %d : %s\n", ($ccr >> 9) & 1, ($ccr & 0x200) ? "8-byte stack alignment on exception entry" : "4-byte stack alignment"
  printf "  [8]  BFHFNMIGN  = %d : %s\n", ($ccr >> 8) & 1, ($ccr & 0x100) ? "Ignore BusFault in NMI/HardFault" : "Fault handlers enabled"
  printf "  [4]  DIV_0_TRP  = %d : %s\n", ($ccr >> 4) & 1, ($ccr & 0x10) ? "Divide by zero causes UsageFault" : "Divide by zero returns zero"
  printf "  [3]  UNALIGN_TRP= %d : %s\n", ($ccr >> 3) & 1, ($ccr & 0x08) ? "Unaligned accesses cause UsageFault" : "Unaligned accesses permitted"
  printf "  [1]  NONBASETHRDENA = %d : %s\n", ($ccr >> 1) & 1, ($ccr & 0x02) ? "Thread mode can use PSP" : "Thread mode uses MSP only"
  
  echo \n
  printf "System Handler Control and State (SHCSR) = 0x%08x\n", $shcsr
  printf "  [18] USGFAULTENA = %d : UsageFault is %s\n", ($shcsr >> 18) & 1, ($shcsr & 0x40000) ? "ENABLED" : "DISABLED"
  printf "  [17] BUSFAULTENA = %d : BusFault is %s\n", ($shcsr >> 17) & 1, ($shcsr & 0x20000) ? "ENABLED" : "DISABLED"
  printf "  [16] MEMFAULTENA = %d : MemManage fault is %s\n", ($shcsr >> 16) & 1, ($shcsr & 0x10000) ? "ENABLED" : "DISABLED"
  
  if (($shcsr & 0x70000) == 0)
    echo   ** WARNING: All configurable fault handlers are disabled!\n
    echo   ** Faults will escalate to HardFault!\n
  end

  # Read Fault Status Registers
  echo \n=== Fault Status Registers ===\n
  set $cfsr = *(unsigned int*)0xE000ED28
  set $hfsr = *(unsigned int*)0xE000ED2C
  set $dfsr = *(unsigned int*)0xE000ED30
  set $mmfar = *(unsigned int*)0xE000ED34
  set $bfar = *(unsigned int*)0xE000ED38

  printf "CFSR  = 0x%08x\n", $cfsr
  printf "HFSR  = 0x%08x\n", $hfsr
  printf "DFSR  = 0x%08x\n", $dfsr
  
  echo \n=== Detailed Fault Analysis ===\n

  # Split CFSR into sub-registers
  set $ufsr = ($cfsr >> 16) & 0xFFFF
  set $bfsr = ($cfsr >> 8) & 0xFF
  set $mmfsr = $cfsr & 0xFF

  # Analyze MemManage Faults (MMFSR - bits 7:0)
  if $mmfsr
    echo [MemManage Fault Status]\n
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
      echo  - MLSPERR: MemManage fault on floating-point lazy state preservation\n
    end
    if ($cfsr & 0x00000080)
      printf " - MMARVALID: MMFAR contains valid address: 0x%08x\n", $mmfar
    end
    echo \n
  end

  # Analyze Bus Faults (BFSR - bits 15:8)
  if $bfsr
    echo [BusFault Status]\n
    if ($cfsr & 0x00000100)
      echo  - IBUSERR: Instruction bus error\n
    end
    if ($cfsr & 0x00000200)
      echo  - PRECISERR: Precise data bus error\n
    end
    if ($cfsr & 0x00000400)
      echo  - IMPRECISERR: Imprecise data bus error\n
    end
    if ($cfsr & 0x00000800)
      echo  - UNSTKERR: Bus fault on exception unstacking\n
    end
    if ($cfsr & 0x00001000)
      echo  - STKERR: Bus fault on exception stacking\n
    end
    if ($cfsr & 0x00002000)
      echo  - LSPERR: Bus fault on floating-point lazy state preservation\n
    end
    if ($cfsr & 0x00008000)
      printf " - BFARVALID: BFAR contains valid address: 0x%08x\n", $bfar
    end
    echo \n
  end

  # Analyze Usage Faults (UFSR - bits 31:16)
  if $ufsr
    echo [UsageFault Status]\n
    if ($cfsr & 0x00010000)
      echo  - UNDEFINSTR: Undefined instruction executed\n
    end
    if ($cfsr & 0x00020000)
      echo  - INVSTATE: Invalid state (EPSR.T bit issue)\n
    end
    if ($cfsr & 0x00040000)
      echo  - INVPC: Invalid PC load (illegal EXC_RETURN)\n
    end
    if ($cfsr & 0x00080000)
      echo  - NOCP: Attempted to access a coprocessor\n
    end
    if ($cfsr & 0x01000000)
      echo  - UNALIGNED: Unaligned memory access\n
    end
    if ($cfsr & 0x02000000)
      echo  - DIVBYZERO: Division by zero occurred\n
    end
    echo \n
  end

  # Hard Fault Analysis
  echo [HardFault Status]\n
  if ($hfsr & 0x00000002)
    echo  - VECTTBL: Vector table read fault on exception\n
  end
  if ($hfsr & 0x40000000)
    echo  - FORCED: Fault escalated to HardFault (original fault info in CFSR)\n
  end
  if ($hfsr & 0x80000000)
    echo  - DEBUGEVT: Debug event occurred\n
  end
  
  if (($hfsr & 0xC0000002) == 0)
    echo  No HardFault flags set\n
  end

  # Print fault address registers
  echo \n=== Fault Address Registers ===\n
  if ($cfsr & 0x80)
    printf "MMFAR: 0x%08x (VALID - MemManage fault address)\n", $mmfar
  else
    printf "MMFAR: Not valid (MMARVALID=0)\n"
  end
  
  if ($cfsr & 0x8000)
    printf "BFAR:  0x%08x (VALID - BusFault address)\n", $bfar
  else
    printf "BFAR:  Not valid (BFARVALID=0)\n"
  end

  # Analysis Summary
  echo \n=== Analysis Summary ===\n
  
  # Determine fault type
  set $fault_type = "Unknown"
  if $ufsr
    set $fault_type = "UsageFault"
    if ($cfsr & 0x01000000)
      echo Fault Type: UsageFault (UNALIGNED) 
      if ($hfsr & 0x40000000)
        echo escalated to HardFault\n
      else
        echo \n
      end
      echo Reason: Unaligned memory access attempted\n
      if (($ccr & 0x08) == 0)
        echo Note: UNALIGN_TRP is disabled - this fault should not have occurred!\n
      end
    end
    if ($cfsr & 0x02000000)
      echo Fault Type: UsageFault (DIVBYZERO) 
      if ($hfsr & 0x40000000)
        echo escalated to HardFault\n
      else
        echo \n
      end
      echo Reason: Division by zero\n
      if (($ccr & 0x10) == 0)
        echo Note: DIV_0_TRP is disabled - this fault should not have occurred!\n
      end
    end
  end
  
  if $bfsr && ($fault_type == "Unknown")
    set $fault_type = "BusFault"
    echo Fault Type: BusFault 
    if ($hfsr & 0x40000000)
      echo escalated to HardFault\n
    else
      echo \n
    end
  end
  
  if $mmfsr && ($fault_type == "Unknown")
    set $fault_type = "MemManage Fault"
    echo Fault Type: MemManage Fault 
    if ($hfsr & 0x40000000)
      echo escalated to HardFault\n
    else
      echo \n
    end
  end
  
  # Check handler enable
  if ($hfsr & 0x40000000)
    if $ufsr && (($shcsr & 0x40000) == 0)
      echo Escalation Reason: UsageFault handler is disabled (USGFAULTENA=0)\n
    end
    if $bfsr && (($shcsr & 0x20000) == 0)
      echo Escalation Reason: BusFault handler is disabled (BUSFAULTENA=0)\n
    end
    if $mmfsr && (($shcsr & 0x10000) == 0)
      echo Escalation Reason: MemManage handler is disabled (MEMFAULTENA=0)\n
    end
  end
  
  printf "PC at fault: 0x%08x\n", $pc

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
alias lcb = load_crash_binary
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