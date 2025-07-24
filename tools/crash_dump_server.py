#!/usr/bin/env python3
"""
TCP Server for receiving STM32H7 crash dumps and creating ELF core dump files
Creates ARM Cortex-M core dumps compatible with GDB for post-mortem debugging
"""

import socket
import struct
import sys
import argparse
import os
from datetime import datetime

class CrashDumpServer:
    def __init__(self, host='0.0.0.0', port=9999):
        self.host = host
        self.port = port
        self.dump_data = bytearray()
        self.registers = {}
        self.memory_regions = []
        
        # STM32H7 memory map
        self.memory_map = [
            {'name': 'DTCM',        'start': 0x20000000, 'end': 0x20020000},
            {'name': 'AXI_SRAM',    'start': 0x24000000, 'end': 0x24080000},
            {'name': 'SRAM1',       'start': 0x30000000, 'end': 0x30020000},
            {'name': 'SRAM2',       'start': 0x30020000, 'end': 0x30040000},
            {'name': 'SRAM3',       'start': 0x30040000, 'end': 0x30048000},
            {'name': 'SRAM4',       'start': 0x38000000, 'end': 0x38010000},
            {'name': 'BACKUP_SRAM', 'start': 0x38800000, 'end': 0x38801000},
        ]
        
    def parse_hexdump_line(self, line):
        """Parse a line of hexdump format: XXXXXXXX: XX XX XX XX ..."""
        line = line.strip()
        if not line or ':' not in line:
            return None, None
            
        try:
            parts = line.split(':', 1)
            if len(parts) != 2:
                return None, None
                
            address = int(parts[0], 16)
            hex_bytes = parts[1].strip().split()
            data = bytes([int(b, 16) for b in hex_bytes])
            return address, data
        except:
            return None, None
    
    def parse_register(self, line):
        """Parse register format: REGNAME: XXXXXXXX"""
        line = line.strip()
        if ':' not in line:
            return None, None
            
        try:
            parts = line.split(':', 1)
            if len(parts) != 2:
                return None, None
                
            reg_name = parts[0].strip()
            value_str = parts[1].strip()
            
            # Clean up the value string - remove spaces and control characters
            # Could be "12345678" or "12 34 56 78" or have control chars
            value_str = ''.join(c for c in value_str if c.isalnum())
            
            # Parse hex value
            value = int(value_str, 16)
            
            # Ensure value fits in 32 bits (handle overflow)
            value = value & 0xFFFFFFFF
            
            return reg_name, value
        except Exception as e:
            return None, None
    
    def receive_dump(self):
        """Receive crash dump from TCP connection"""
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
            server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            server.bind((self.host, self.port))
            server.listen(1)
            
            print(f"Crash dump server listening on {self.host}:{self.port}")
            
            while True:
                conn, addr = server.accept()
                print(f"Connection from {addr}")
                
                # Reset for new dump
                self.dump_data = bytearray()
                self.registers = {}
                self.memory_regions = []
                current_region = None
                in_memory_dump = False
                
                try:
                    while True:
                        data = conn.recv(4096)
                        if not data:
                            break
                            
                        # Decode and process line by line
                        self.dump_data.extend(data)
                        lines = self.dump_data.decode('utf-8', errors='ignore').split('\n')
                        
                        # Keep incomplete line for next iteration
                        if not data.endswith(b'\n'):
                            self.dump_data = lines[-1].encode('utf-8')
                            lines = lines[:-1]
                        else:
                            self.dump_data = bytearray()
                        
                        for line in lines:
                            line = line.rstrip('\r\n')
                            
                            if "CRASH ENCOUNTERED" in line:
                                print("Crash dump started")
                                continue
                                
                            if "Memory dump:" in line:
                                in_memory_dump = True
                                continue
                                
                            if "End of dump" in line:
                                print("Dump complete")
                                break
                            
                            # Parse registers
                            if not in_memory_dump and ':' in line:
                                reg_name, value = self.parse_register(line)
                                if reg_name and value is not None:
                                    self.registers[reg_name] = value
                            
                            # Parse memory regions
                            if in_memory_dump:
                                # Check for region header (e.g., "DTCM:")
                                if line.endswith(':') and not line[0].isdigit():
                                    region_name = line[:-1]
                                    current_region = {'name': region_name, 'data': {}}
                                    self.memory_regions.append(current_region)
                                    print(f"\nRegion: {region_name}")
                                elif current_region and ':' in line:
                                    # Parse memory line
                                    addr, data = self.parse_hexdump_line(line)
                                    if addr is not None and data:
                                        current_region['data'][addr] = data
                        
                except Exception as e:
                    print(f"Error receiving dump: {e}")
                finally:
                    conn.close()
                    
                # Generate core dump if we got data
                if self.registers and self.memory_regions:
                    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                    core_filename = f"core_{timestamp}.elf"
                    bin_filename = f"dump_{timestamp}.bin"
                    elf_filename = "app.elf"  # User should provide their app ELF
                    
                    self.print_crash_summary()
                    self.create_core_dump(core_filename)
                    self.create_binary_dump(bin_filename)
                    
                    print(f"\n[SUCCESS] Core dumps created:")
                    print(f"  - ELF format: {core_filename}")
                    print(f"  - Binary format: {bin_filename}")
                    print(f"\nFor GDB analysis of ELF:")
                    print(f"1. Copy your application ELF to '{elf_filename}'")
                    print(f"2. Run: arm-none-eabi-gdb {elf_filename}")
                    print(f"3. In GDB: set architecture arm")
                    print(f"4. In GDB: core {core_filename}")
                    print(f"\nFor loading binary dump into live target:")
                    print(f"1. Connect to target with GDB")
                    print(f"2. Run: load_crash_binary {bin_filename}")
                    
    def print_crash_summary(self):
        """Print summary of crash information"""
        print("\n" + "="*60)
        print("CRASH DUMP ANALYSIS")
        print("="*60)
        
        # Print registers
        print("\nCPU REGISTERS:")
        print("-" * 40)
        for i in range(0, 13, 4):
            line = ""
            for j in range(4):
                if i+j < 13:
                    reg = f"R{i+j}"
                    if reg in self.registers:
                        line += f"{reg:3}: 0x{self.registers[reg]:08X}  "
            print(line)
        
        special_regs = ['SP', 'LR', 'PC', 'PSR']
        line = ""
        for reg in special_regs:
            if reg in self.registers:
                line += f"{reg:3}: 0x{self.registers[reg]:08X}  "
        print(line)
        
        print("\nFAULT REGISTERS:")
        print("-" * 40)
        fault_regs = ['HFSR', 'CFSR', 'MMFAR', 'BFAR', 'AFSR']
        for reg in fault_regs:
            if reg in self.registers:
                print(f"{reg:6}: 0x{self.registers[reg]:08X}")
        
        # Decode CFSR if available
        if 'CFSR' in self.registers:
            cfsr = self.registers['CFSR']
            if cfsr & 0x00010000:
                print("  -> UsageFault: Divide by zero")
            if cfsr & 0x00020000:
                print("  -> UsageFault: Unaligned access")
            if cfsr & 0x00008000:
                print("  -> BusFault: Precise data access violation")
            if cfsr & 0x00001000:
                print("  -> BusFault: Imprecise data access violation")
            if cfsr & 0x00000080:
                print("  -> MemManage: Data access violation")
            if cfsr & 0x00000001:
                print("  -> MemManage: Instruction access violation")
        
        # Print memory regions received
        print("\nMEMORY REGIONS RECEIVED:")
        print("-" * 40)
        total_size = 0
        for region in self.memory_regions:
            size = 0
            for addr, data in region['data'].items():
                size += len(data)
            total_size += size
            print(f"{region['name']:12}: {size:8} bytes")
        print(f"{'TOTAL':12}: {total_size:8} bytes ({total_size/1024:.1f} KB)")
        print("="*60)
        
    def create_core_dump(self, filename):
        """Create ELF core dump file compatible with GDB"""
        # ELF header for ARM Cortex-M7 core dump
        e_ident = b'\x7fELF'  # Magic
        e_ident += b'\x01'    # 32-bit
        e_ident += b'\x01'    # Little endian
        e_ident += b'\x01'    # Current version
        e_ident += b'\x00' * 9  # Padding
        
        # ELF header fields for core dump
        e_type = 0x0004      # ET_CORE (core dump)
        e_machine = 0x0028   # EM_ARM
        e_version = 0x00000001
        e_entry = 0          # No entry point for core dumps
        e_phoff = 0x34       # Program header offset (52 bytes)
        e_shoff = 0          # No section headers
        e_flags = 0x05000200 # ARM EABI v5 + has entry point
        e_ehsize = 0x34      # ELF header size (52 bytes)
        e_phentsize = 0x20   # Program header entry size (32 bytes)
        e_phnum = 0          # Will be calculated
        e_shentsize = 0
        e_shnum = 0
        e_shstrndx = 0
        
        # Build memory segments
        segments = []
        
        # Create register note only (simplest approach)
        notes = self.create_register_note()
        
        # Add notes segment
        segments.append({
            'type': 0x04,  # PT_NOTE
            'offset': 0,
            'vaddr': 0,
            'paddr': 0,
            'filesz': len(notes),
            'memsz': len(notes),
            'flags': 0x04,  # PF_R
            'align': 4,
            'data': notes
        })
        
        # Add memory segments
        for region in self.memory_regions:
            if region['data']:
                # Find continuous memory blocks
                addresses = sorted(region['data'].keys())
                if not addresses:
                    continue
                    
                start_addr = addresses[0]
                data = bytearray()
                expected_addr = start_addr
                
                for addr in addresses:
                    if addr != expected_addr:
                        # Gap in memory, create segment
                        if data:
                            segments.append({
                                'type': 0x01,  # PT_LOAD
                                'offset': 0,
                                'vaddr': start_addr,
                                'paddr': start_addr,
                                'filesz': len(data),
                                'memsz': len(data),
                                'flags': 0x07,  # PF_R | PF_W | PF_X
                                'align': 4,
                                'data': data
                            })
                        start_addr = addr
                        data = bytearray()
                        expected_addr = addr
                    
                    data.extend(region['data'][addr])
                    expected_addr = addr + len(region['data'][addr])
                
                # Add final segment
                if data:
                    segments.append({
                        'type': 0x01,  # PT_LOAD
                        'offset': 0,
                        'vaddr': start_addr,
                        'paddr': start_addr,
                        'filesz': len(data),
                        'memsz': len(data),
                        'flags': 0x07,  # PF_R | PF_W | PF_X
                        'align': 4,
                        'data': data
                    })
        
        # Calculate offsets
        current_offset = 0x34 + len(segments) * 0x20  # After headers
        for seg in segments:
            seg['offset'] = current_offset
            current_offset += seg['filesz']
            # Align to 4 bytes
            if current_offset % 4:
                current_offset += 4 - (current_offset % 4)
        
        # Write ELF file
        with open(filename, 'wb') as f:
            # Write ELF header
            f.write(e_ident)
            f.write(struct.pack('<H', e_type))
            f.write(struct.pack('<H', e_machine))
            f.write(struct.pack('<I', e_version))
            f.write(struct.pack('<I', e_entry))
            f.write(struct.pack('<I', e_phoff))
            f.write(struct.pack('<I', e_shoff))
            f.write(struct.pack('<I', e_flags))
            f.write(struct.pack('<H', e_ehsize))
            f.write(struct.pack('<H', e_phentsize))
            f.write(struct.pack('<H', len(segments)))
            f.write(struct.pack('<H', e_shentsize))
            f.write(struct.pack('<H', e_shnum))
            f.write(struct.pack('<H', e_shstrndx))
            
            # Write program headers
            for seg in segments:
                f.write(struct.pack('<I', seg['type']))     # p_type
                f.write(struct.pack('<I', seg['offset']))   # p_offset
                f.write(struct.pack('<I', seg['vaddr']))    # p_vaddr
                f.write(struct.pack('<I', seg['paddr']))    # p_paddr
                f.write(struct.pack('<I', seg['filesz']))   # p_filesz
                f.write(struct.pack('<I', seg['memsz']))    # p_memsz
                f.write(struct.pack('<I', seg['flags']))    # p_flags
                f.write(struct.pack('<I', seg['align']))    # p_align
            
            # Write segment data
            for seg in segments:
                f.seek(seg['offset'])
                f.write(seg['data'])
    
    def create_register_note(self):
        """Create PT_NOTE segment with register values for GDB"""
        # Note name "CORE\0"
        name = b'CORE\0'
        while len(name) % 4:
            name += b'\0'
        
        # ARM elf_prstatus structure for bare metal
        # Based on Linux kernel but simplified for bare metal
        prstatus = bytearray()
        
        # struct elf_prstatus {
        #   struct elf_siginfo pr_info;  /* 12 bytes */
        #   short pr_cursig;             /* 2 bytes */
        #   unsigned long pr_sigpend;    /* 4 bytes */  
        #   unsigned long pr_sighold;    /* 4 bytes */
        #   pid_t pr_pid;                /* 4 bytes */
        #   pid_t pr_ppid;               /* 4 bytes */
        #   pid_t pr_pgrp;               /* 4 bytes */
        #   pid_t pr_sid;                /* 4 bytes */
        #   struct timeval pr_utime;     /* 8 bytes */
        #   struct timeval pr_stime;     /* 8 bytes */
        #   struct timeval pr_cutime;    /* 8 bytes */
        #   struct timeval pr_cstime;    /* 8 bytes */
        #   elf_gregset_t pr_reg;        /* GP registers */
        #   int pr_fpvalid;              /* 4 bytes */
        # };
        
        # pr_info (12 bytes) - signal info
        prstatus.extend(struct.pack('<i', 11))    # si_signo = SIGSEGV
        prstatus.extend(struct.pack('<i', 0))     # si_code
        prstatus.extend(struct.pack('<i', 0))     # si_errno
        
        # pr_cursig (2 bytes)
        prstatus.extend(struct.pack('<h', 11))    # SIGSEGV
        
        # Padding to align to 4 bytes
        prstatus.extend(struct.pack('<h', 0))
        
        # pr_sigpend, pr_sighold (8 bytes)
        prstatus.extend(struct.pack('<II', 0, 0))
        
        # pr_pid, pr_ppid, pr_pgrp, pr_sid (16 bytes)
        prstatus.extend(struct.pack('<IIII', 1, 0, 1, 1))
        
        # pr_utime, pr_stime, pr_cutime, pr_cstime (32 bytes)
        prstatus.extend(struct.pack('<QQQQ', 0, 0, 0, 0))
        
        # pr_reg - ARM register set (18 registers * 4 bytes = 72 bytes)
        # Order: r0-r15, cpsr, orig_r0
        for i in range(13):
            reg_name = f'R{i}'
            value = self.registers.get(reg_name, 0)
            prstatus.extend(struct.pack('<I', value))
        
        # Special registers - ensure they fit in 32 bits
        sp_val = self.registers.get('SP', 0) & 0xFFFFFFFF
        lr_val = self.registers.get('LR', 0) & 0xFFFFFFFF
        pc_val = self.registers.get('PC', 0) & 0xFFFFFFFF
        psr_val = self.registers.get('PSR', 0) & 0xFFFFFFFF
        
        prstatus.extend(struct.pack('<I', sp_val))   # r13
        prstatus.extend(struct.pack('<I', lr_val))   # r14
        prstatus.extend(struct.pack('<I', pc_val))   # r15
        prstatus.extend(struct.pack('<I', psr_val))  # cpsr
        prstatus.extend(struct.pack('<I', 0))         # orig_r0
        
        # pr_fpvalid (4 bytes)
        prstatus.extend(struct.pack('<I', 0))
        
        # Note header
        note = bytearray()
        note.extend(struct.pack('<I', len(name)))      # namesz
        note.extend(struct.pack('<I', len(prstatus)))  # descsz  
        note.extend(struct.pack('<I', 1))              # NT_PRSTATUS
        note.extend(name)
        note.extend(prstatus)
        
        # Pad to 4-byte alignment
        while len(note) % 4:
            note.append(0)
        
        return note
    
    def create_thread_note(self):
        """Create thread info note for GDB"""
        name = b'CORE\0'
        while len(name) % 4:
            name += b'\0'
            
        # prpsinfo structure (minimal)
        prpsinfo = bytearray()
        
        # state, sname, zomb, nice
        prpsinfo.extend(struct.pack('<BBBB', 0, 0, 0, 0))
        
        # flag, uid, gid
        prpsinfo.extend(struct.pack('<III', 0, 0, 0))
        
        # pid, ppid, pgrp, sid
        prpsinfo.extend(struct.pack('<IIII', 1, 0, 1, 1))
        
        # fname (16 bytes) - command name
        fname = b'stm32h7\0'
        prpsinfo.extend(fname.ljust(16, b'\0'))
        
        # psargs (80 bytes) - command line
        psargs = b'stm32h7 app\0'
        prpsinfo.extend(psargs.ljust(80, b'\0'))
        
        # Note header
        note = bytearray()
        note.extend(struct.pack('<I', len(name)))      # namesz
        note.extend(struct.pack('<I', len(prpsinfo)))  # descsz
        note.extend(struct.pack('<I', 3))              # NT_PRPSINFO
        note.extend(name)
        note.extend(prpsinfo)
        
        # Pad to 4-byte alignment
        while len(note) % 4:
            note.append(0)
            
        return note
    
    def create_auxv_note(self):
        """Create auxiliary vector note (helps GDB understand the target)"""
        name = b'CORE\0'
        while len(name) % 4:
            name += b'\0'
        
        # Minimal auxv entries for ARM
        auxv = bytearray()
        
        # AT_HWCAP = 16, Cortex-M7 capabilities
        auxv.extend(struct.pack('<II', 16, 0x00001000))  # Has Thumb
        
        # AT_PAGESZ = 6, page size
        auxv.extend(struct.pack('<II', 6, 4096))
        
        # AT_NULL = 0, end marker
        auxv.extend(struct.pack('<II', 0, 0))
        
        # Note header
        note = bytearray()
        note.extend(struct.pack('<I', len(name)))     # namesz
        note.extend(struct.pack('<I', len(auxv)))     # descsz
        note.extend(struct.pack('<I', 6))             # NT_AUXV
        note.extend(name)
        note.extend(auxv)
        
        # Pad to 4-byte alignment
        while len(note) % 4:
            note.append(0)
            
        return note
    
    def create_binary_dump(self, filename):
        """Create binary dump compatible with GDB script load_crash_binary"""
        print(f"\nCreating binary dump: {filename}")
        
        with open(filename, 'wb') as f:
            # Write all memory regions in order expected by GDB script
            # The script expects specific offsets for each region
            
            # Calculate total size needed
            total_size = 0x0d91bc  # End of last region in GDB script
            
            # Create buffer filled with 0xFF (unprogrammed flash pattern)
            dump_data = bytearray(b'\xFF' * total_size)
            
            # Map our regions to GDB script expectations
            region_mapping = {
                'DTCM': {'start': 0x20000000, 'file_offset': 0x0001bc, 'size': 0x80000},
                'AXI_SRAM': {'start': 0x24000000, 'file_offset': 0x0001bc, 'size': 0x80000},
                'SRAM1': {'start': 0x30000000, 'file_offset': 0x0801bc, 'size': 0x20000},
                'SRAM2': {'start': 0x30020000, 'file_offset': 0x0a01bc, 'size': 0x20000},
                'SRAM3': {'start': 0x30040000, 'file_offset': 0x0c01bc, 'size': 0x08000},
                'SRAM4': {'start': 0x38000000, 'file_offset': 0x0c81bc, 'size': 0x10000},
                'BACKUP_SRAM': {'start': 0x38800000, 'file_offset': 0x0d81bc, 'size': 0x1000},
            }
            
            # Fill memory regions
            for region in self.memory_regions:
                region_name = region['name']
                if region_name in region_mapping:
                    mapping = region_mapping[region_name]
                    
                    # Sort addresses and write data
                    for addr, data in sorted(region['data'].items()):
                        offset = addr - mapping['start']
                        file_pos = mapping['file_offset'] + offset
                        
                        # Write data at correct position
                        if file_pos + len(data) <= len(dump_data):
                            dump_data[file_pos:file_pos + len(data)] = data
            
            # Write registers at offset 0x114 (as expected by GDB script)
            # Format: prstatus structure with registers at offset 72
            reg_offset = 0x114
            
            # Create minimal prstatus structure
            prstatus = bytearray(148)  # Size from GDB script (0x1a8 - 0x114)
            
            # Skip to register offset (72 bytes into prstatus)
            reg_pos = 72
            
            # Write registers in order expected by GDB
            for i in range(13):
                reg_name = f'R{i}'
                value = self.registers.get(reg_name, 0)
                struct.pack_into('<I', prstatus, reg_pos + i*4, value)
            
            # Special registers
            struct.pack_into('<I', prstatus, reg_pos + 13*4, self.registers.get('SP', 0))
            struct.pack_into('<I', prstatus, reg_pos + 14*4, self.registers.get('LR', 0))
            struct.pack_into('<I', prstatus, reg_pos + 15*4, self.registers.get('PC', 0))
            struct.pack_into('<I', prstatus, reg_pos + 16*4, self.registers.get('PSR', 0))
            
            # Write prstatus at expected offset
            dump_data[reg_offset:reg_offset + len(prstatus)] = prstatus
            
            # Write to file
            f.write(dump_data)
            
        print(f"Binary dump created: {os.path.getsize(filename)} bytes")

def main():
    parser = argparse.ArgumentParser(description='STM32H7 Crash Dump Server - Creates GDB-compatible core dumps')
    parser.add_argument('--host', default='0.0.0.0', help='Host to listen on (default: 0.0.0.0)')
    parser.add_argument('--port', type=int, default=9999, help='Port to listen on (default: 9999)')
    
    args = parser.parse_args()
    
    print(f"STM32H7 Crash Dump Server")
    print(f"Creates ARM Cortex-M core dumps for GDB analysis")
    print(f"Listening on {args.host}:{args.port}")
    print(f"Press Ctrl+C to stop\n")
    
    server = CrashDumpServer(args.host, args.port)
    try:
        server.receive_dump()
    except KeyboardInterrupt:
        print("\nServer stopped")

if __name__ == '__main__':
    main()