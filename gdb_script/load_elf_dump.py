#!/usr/bin/env python3
"""
GDB Python script to load ELF core dumps into STM32H7 target
"""

import gdb
import struct
import sys

class ElfCoreLoader(gdb.Command):
    """Load ELF core dump into connected STM32H7 target"""
    
    def __init__(self):
        super(ElfCoreLoader, self).__init__("load-elf-dump", gdb.COMMAND_USER)
        
    def invoke(self, arg, from_tty):
        args = gdb.string_to_argv(arg)
        if len(args) != 1:
            print("Usage: load-elf-dump <path_to_core.elf>")
            return
            
        elf_path = args[0]
        print(f"\n=== Loading STM32H7 ELF core dump ===")
        print(f"File: {elf_path}\n")
        
        try:
            with open(elf_path, 'rb') as f:
                # Read ELF header
                f.seek(0)
                elf_header = f.read(52)  # 32-bit ELF header size
                
                # Parse basic ELF info
                magic = elf_header[0:4]
                if magic != b'\x7fELF':
                    print("ERROR: Not a valid ELF file")
                    return
                    
                # Get program header info
                e_phoff = struct.unpack('<I', elf_header[28:32])[0]
                e_phentsize = struct.unpack('<H', elf_header[42:44])[0]
                e_phnum = struct.unpack('<H', elf_header[44:46])[0]
                
                print(f"Program headers: {e_phnum} entries at offset 0x{e_phoff:x}")
                
                # Process each program header
                for i in range(e_phnum):
                    f.seek(e_phoff + i * e_phentsize)
                    ph = f.read(e_phentsize)
                    
                    p_type = struct.unpack('<I', ph[0:4])[0]
                    p_offset = struct.unpack('<I', ph[4:8])[0]
                    p_vaddr = struct.unpack('<I', ph[8:12])[0]
                    p_paddr = struct.unpack('<I', ph[12:16])[0]
                    p_filesz = struct.unpack('<I', ph[16:20])[0]
                    
                    # PT_NOTE = 4
                    if p_type == 4:
                        print(f"\n[{i+1}/{e_phnum}] PT_NOTE segment - parsing registers...")
                        self.load_registers_from_note(f, p_offset, p_filesz)
                    
                    # PT_LOAD = 1
                    elif p_type == 1 and p_filesz > 0:
                        region_name = self.get_region_name(p_vaddr)
                        print(f"\n[{i+1}/{e_phnum}] PT_LOAD: {region_name} @ 0x{p_vaddr:08x} ({p_filesz} bytes)")
                        
                        # Read segment data
                        f.seek(p_offset)
                        data = f.read(p_filesz)
                        
                        # Write to target memory
                        self.write_memory(p_vaddr, data)
                        
                print("\n=== Load complete ===")
                
                # Show restored state
                gdb.execute("info registers")
                
                # Analyze crash
                print("\n=== Analyzing crash ===")
                gdb.execute("analyze_crash")
                
        except Exception as e:
            print(f"ERROR: {e}")
            import traceback
            traceback.print_exc()
    
    def get_region_name(self, addr):
        """Get memory region name from address"""
        regions = {
            0x00000000: "ITCM",
            0x20000000: "DTCM", 
            0x24000000: "AXI_SRAM",
            0x30000000: "SRAM1",
            0x30020000: "SRAM2",
            0x30040000: "SRAM3",
            0x38000000: "SRAM4",
            0x38800000: "BACKUP_SRAM",
        }
        
        for base, name in sorted(regions.items(), reverse=True):
            if addr >= base:
                return name
        return f"Unknown_0x{addr:08x}"
    
    def write_memory(self, addr, data):
        """Write data to target memory"""
        try:
            # GDB inferior is the target
            inferior = gdb.selected_inferior()
            
            # Write in chunks to avoid timeout
            chunk_size = 1024
            for offset in range(0, len(data), chunk_size):
                chunk = data[offset:offset + chunk_size]
                inferior.write_memory(addr + offset, chunk)
                
                # Show progress
                if offset % (chunk_size * 16) == 0:
                    print(f"  Written {offset}/{len(data)} bytes...")
                    
            print(f"  Written {len(data)} bytes successfully")
            
        except gdb.MemoryError as e:
            print(f"  ERROR writing memory: {e}")
            
    def load_registers_from_note(self, f, offset, size):
        """Parse PT_NOTE segment and restore registers"""
        f.seek(offset)
        note_data = f.read(size)
        
        pos = 0
        while pos < size:
            # Note header
            if pos + 12 > size:
                break
                
            namesz = struct.unpack('<I', note_data[pos:pos+4])[0]
            descsz = struct.unpack('<I', note_data[pos+4:pos+8])[0]
            note_type = struct.unpack('<I', note_data[pos+8:pos+12])[0]
            pos += 12
            
            # Name (aligned to 4 bytes)
            name = note_data[pos:pos+namesz].rstrip(b'\0')
            pos += (namesz + 3) & ~3
            
            # NT_PRSTATUS = 1
            if note_type == 1 and name == b'CORE':
                print("  Found PRSTATUS note - restoring registers...")
                
                # Skip to registers in prstatus structure
                # Offset 72 is where registers start in our structure
                reg_offset = 72
                if reg_offset + 72 <= descsz:  # 18 registers * 4 bytes
                    reg_data = note_data[pos + reg_offset:pos + reg_offset + 72]
                    
                    # Parse registers
                    regs = struct.unpack('<18I', reg_data)
                    
                    # Set registers in GDB
                    reg_names = ['r0', 'r1', 'r2', 'r3', 'r4', 'r5', 'r6', 'r7',
                                'r8', 'r9', 'r10', 'r11', 'r12', 'sp', 'lr', 'pc', 
                                'xpsr']
                    
                    for i, (name, value) in enumerate(zip(reg_names, regs[:17])):
                        try:
                            gdb.execute(f"set ${name} = 0x{value:08x}")
                            print(f"    ${name} = 0x{value:08x}")
                        except:
                            print(f"    WARNING: Could not set ${name}")
                            
                print("  Registers restored")
                
            # Skip descriptor (aligned to 4 bytes)
            pos += (descsz + 3) & ~3

# Register the command
ElfCoreLoader()

print("ELF core dump loader ready. Use 'load-elf-dump <file>' to load a dump.")