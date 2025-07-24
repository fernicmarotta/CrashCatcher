#!/usr/bin/env python3
"""
Test script to create a minimal ARM core dump that GDB can load
"""

import struct

def create_minimal_arm_core():
    """Create the most minimal ARM core dump"""
    
    # Test registers
    registers = {
        'R0': 0x20001234,
        'R1': 0x00000001,
        'R2': 0xDEADBEEF,
        'R3': 0x00000000,
        'R4': 0x00000000,
        'R5': 0x00000000,
        'R6': 0x00000000,
        'R7': 0x00000000,
        'R8': 0x00000000,
        'R9': 0x00000000,
        'R10': 0x00000000,
        'R11': 0x00000000,
        'R12': 0x00000000,
        'SP': 0x20010000,
        'LR': 0x08001234,
        'PC': 0x08005678,
        'PSR': 0x61000000,
    }
    
    # Create NT_PRSTATUS note
    note_name = b'CORE\0\0\0\0'  # 8 bytes aligned
    
    # ARM Linux prstatus structure (simplified)
    prstatus = bytearray()
    
    # Signal info (12 bytes)
    prstatus.extend(struct.pack('<iii', 11, 0, 0))  # SIGSEGV
    
    # pr_cursig (2 bytes) + padding (2 bytes)
    prstatus.extend(struct.pack('<HH', 11, 0))
    
    # pr_sigpend, pr_sighold (8 bytes)
    prstatus.extend(struct.pack('<II', 0, 0))
    
    # PIDs (16 bytes)
    prstatus.extend(struct.pack('<IIII', 1, 0, 1, 1))
    
    # Times (32 bytes)
    prstatus.extend(b'\0' * 32)
    
    # Registers (18 * 4 = 72 bytes)
    # r0-r12
    for i in range(13):
        prstatus.extend(struct.pack('<I', registers.get(f'R{i}', 0)))
    # sp, lr, pc
    prstatus.extend(struct.pack('<I', registers['SP']))
    prstatus.extend(struct.pack('<I', registers['LR']))
    prstatus.extend(struct.pack('<I', registers['PC']))
    # cpsr
    prstatus.extend(struct.pack('<I', registers['PSR']))
    # orig_r0
    prstatus.extend(struct.pack('<I', 0))
    
    # pr_fpvalid
    prstatus.extend(struct.pack('<I', 0))
    
    # Create note
    note = bytearray()
    note.extend(struct.pack('<III', 8, len(prstatus), 1))  # namesz, descsz, NT_PRSTATUS
    note.extend(note_name)
    note.extend(prstatus)
    
    # Create simple memory segment (some stack data)
    stack_addr = 0x2000FF00
    stack_data = b'Hello World!\0\0\0\0' + b'\xDE\xAD\xBE\xEF' * 16
    
    # Build ELF
    elf = bytearray()
    
    # ELF header
    elf.extend(b'\x7fELF\x01\x01\x01\0' + b'\0' * 8)  # e_ident
    elf.extend(struct.pack('<HHIIIIIHHHHHH',
        4,      # e_type = ET_CORE
        40,     # e_machine = EM_ARM
        1,      # e_version
        0,      # e_entry
        52,     # e_phoff
        0,      # e_shoff
        0x5000200,  # e_flags (ARM EABI v5 + has entry)
        52,     # e_ehsize
        32,     # e_phentsize
        2,      # e_phnum (NOTE + LOAD)
        0,      # e_shentsize
        0,      # e_shnum
        0       # e_shstrndx
    ))
    
    # Program headers
    # PT_NOTE
    note_offset = 52 + 2 * 32  # After headers
    elf.extend(struct.pack('<IIIIIIII',
        4,              # p_type = PT_NOTE
        note_offset,    # p_offset
        0,              # p_vaddr
        0,              # p_paddr
        len(note),      # p_filesz
        len(note),      # p_memsz
        4,              # p_flags = PF_R
        4               # p_align
    ))
    
    # PT_LOAD (stack memory)
    load_offset = note_offset + len(note)
    if load_offset % 4:
        load_offset += 4 - (load_offset % 4)
    
    elf.extend(struct.pack('<IIIIIIII',
        1,              # p_type = PT_LOAD
        load_offset,    # p_offset
        stack_addr,     # p_vaddr
        stack_addr,     # p_paddr
        len(stack_data),# p_filesz
        len(stack_data),# p_memsz
        6,              # p_flags = PF_R | PF_W
        4               # p_align
    ))
    
    # Add note
    elf.extend(note)
    
    # Pad to load offset
    while len(elf) < load_offset:
        elf.append(0)
    
    # Add memory data
    elf.extend(stack_data)
    
    # Write file
    with open('test_core.elf', 'wb') as f:
        f.write(elf)
    
    print("Created test_core.elf")
    print("\nTo test:")
    print("1. arm-none-eabi-gdb")
    print("2. (gdb) set architecture arm")
    print("3. (gdb) core test_core.elf")
    print("4. (gdb) info registers")

if __name__ == '__main__':
    create_minimal_arm_core()