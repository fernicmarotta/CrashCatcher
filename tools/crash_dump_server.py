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
                    gdb_filename = f"dump_{timestamp}.gdb"
                    elf_filename = "app.elf"  # User should provide their app ELF
                    
                    self.print_crash_summary()
                    self.create_core_dump(core_filename)
                    self.create_binary_dump(bin_filename)
                    self.create_gdb_script(gdb_filename, bin_filename)
                    
                    print(f"\n[SUCCESS] Core dumps created:")
                    print(f"  - ELF format: {core_filename}")
                    print(f"  - Binary format: {bin_filename}")
                    print(f"  - GDB script: {gdb_filename}")
                    print(f"\nFor GDB analysis of ELF:")
                    print(f"1. Copy your application ELF to '{elf_filename}'")
                    print(f"2. Run: arm-none-eabi-gdb {elf_filename}")
                    print(f"3. In GDB: set architecture armv7e-m")
                    print(f"4. In GDB: core {core_filename}")
                    print(f"\nFor loading dump into live target:")
                    print(f"1. Connect to target with GDB")
                    print(f"2. Run: source {gdb_filename}")
                    print(f"\nFor manual binary loading:")
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
        
        # Show MSP and PSP
        stack_regs = ['MSP', 'PSP']
        line = ""
        for reg in stack_regs:
            if reg in self.registers:
                line += f"{reg:3}: 0x{self.registers[reg]:08X}  "
        if line:
            print(line)
        
        # Show LR_AT_FAULT if present
        if 'LR_AT_FAULT' in self.registers:
            print(f"LR_AT_FAULT: 0x{self.registers['LR_AT_FAULT']:08X} (EXC_RETURN)")
        
        # Show control registers
        control_regs = ['CONTROL', 'BASEPRI', 'PRIMASK', 'FAULTMASK', 'FPSCR']
        print("\nCONTROL REGISTERS:")
        print("-" * 40)
        for reg in control_regs:
            if reg in self.registers:
                print(f"{reg:10}: 0x{self.registers[reg]:08X}")
        
        # Show NVIC state
        if 'NVIC_ISER0' in self.registers:
            print("\nNVIC STATE:")
            print("-" * 40)
            print(f"NVIC_ISER0: 0x{self.registers['NVIC_ISER0']:08X} (Enabled interrupts)")
        
        print("\nFAULT STATUS REGISTERS:")
        print("-" * 40)
        
        # HFSR Analysis
        if 'HFSR' in self.registers:
            hfsr = self.registers['HFSR']
            print(f"HFSR  : 0x{hfsr:08X} (HardFault Status Register)")
            print(f"        [31] DEBUGEVT  = {(hfsr >> 31) & 1} : {'Debug event occurred' if hfsr & 0x80000000 else 'No debug event occurred'}")
            print(f"        [30] FORCED    = {(hfsr >> 30) & 1} : {'Fault escalated to HardFault' if hfsr & 0x40000000 else 'No escalation'}")
            print(f"        [1]  VECTTBL   = {(hfsr >> 1) & 1} : {'Vector table read fault' if hfsr & 0x00000002 else 'No vector table read fault'}")
            print()
        
        # CFSR Analysis
        if 'CFSR' in self.registers:
            cfsr = self.registers['CFSR']
            ufsr = (cfsr >> 16) & 0xFFFF
            bfsr = (cfsr >> 8) & 0xFF
            mmfsr = cfsr & 0xFF
            
            print(f"CFSR  : 0x{cfsr:08X} (Configurable Fault Status Register)")
            print(f"        ")
            print(f"        UFSR (UsageFault Status Register) = 0x{ufsr:04X}")
            print(f"        [25] DIVBYZERO = {(cfsr >> 25) & 1} : {'Divide by zero occurred' if cfsr & 0x02000000 else 'No divide by zero'}")
            print(f"        [24] UNALIGNED = {(cfsr >> 24) & 1} : {'Unaligned memory access' if cfsr & 0x01000000 else 'No unaligned access'}")
            print(f"        [19] NOCP      = {(cfsr >> 19) & 1} : {'Coprocessor access' if cfsr & 0x00080000 else 'No coprocessor usage fault'}")
            print(f"        [18] INVPC     = {(cfsr >> 18) & 1} : {'Invalid PC load' if cfsr & 0x00040000 else 'No invalid PC load'}")
            print(f"        [17] INVSTATE  = {(cfsr >> 17) & 1} : {'Invalid state' if cfsr & 0x00020000 else 'No invalid state'}")
            print(f"        [16] UNDEFINSTR= {(cfsr >> 16) & 1} : {'Undefined instruction' if cfsr & 0x00010000 else 'No undefined instruction'}")
            print(f"        ")
            print(f"        BFSR (BusFault Status Register) = 0x{bfsr:02X}")
            print(f"        [15] BFARVALID = {(cfsr >> 15) & 1} : {'BFAR contains valid address' if cfsr & 0x00008000 else 'BFAR does not contain valid address'}")
            print(f"        [13] LSPERR    = {(cfsr >> 13) & 1} : {'Lazy state preservation error' if cfsr & 0x00002000 else 'No lazy state preservation error'}")
            print(f"        [12] STKERR    = {(cfsr >> 12) & 1} : {'Stacking error' if cfsr & 0x00001000 else 'No stacking error'}")
            print(f"        [11] UNSTKERR  = {(cfsr >> 11) & 1} : {'Unstacking error' if cfsr & 0x00000800 else 'No unstacking error'}")
            print(f"        [10] IMPRECISERR= {(cfsr >> 10) & 1} : {'Imprecise data access error' if cfsr & 0x00000400 else 'No imprecise data access error'}")
            print(f"        [9]  PRECISERR = {(cfsr >> 9) & 1} : {'Precise data access error' if cfsr & 0x00000200 else 'No precise data access error'}")
            print(f"        [8]  IBUSERR   = {(cfsr >> 8) & 1} : {'Instruction bus error' if cfsr & 0x00000100 else 'No instruction bus error'}")
            print(f"        ")
            print(f"        MMFSR (MemManage Fault Status Register) = 0x{mmfsr:02X}")
            print(f"        [7]  MMARVALID = {(cfsr >> 7) & 1} : {'MMFAR contains valid address' if cfsr & 0x00000080 else 'MMFAR does not contain valid address'}")
            print(f"        [5]  MLSPERR   = {(cfsr >> 5) & 1} : {'Lazy state preservation error' if cfsr & 0x00000020 else 'No lazy state preservation error'}")
            print(f"        [4]  MSTKERR   = {(cfsr >> 4) & 1} : {'Stacking error' if cfsr & 0x00000010 else 'No stacking error'}")
            print(f"        [3]  MUNSTKERR = {(cfsr >> 3) & 1} : {'Unstacking error' if cfsr & 0x00000008 else 'No unstacking error'}")
            print(f"        [1]  DACCVIOL  = {(cfsr >> 1) & 1} : {'Data access violation' if cfsr & 0x00000002 else 'No data access violation'}")
            print(f"        [0]  IACCVIOL  = {cfsr & 1} : {'Instruction access violation' if cfsr & 0x00000001 else 'No instruction access violation'}")
            print()
        
        # Print address registers
        if 'MMFAR' in self.registers:
            mmfar = self.registers['MMFAR']
            valid = " - VALID" if 'CFSR' in self.registers and self.registers['CFSR'] & 0x00000080 else " - NOT VALID"
            print(f"MMFAR : 0x{mmfar:08X} (MemManage Fault Address Register{valid})")
        
        if 'BFAR' in self.registers:
            bfar = self.registers['BFAR']
            valid = " - VALID" if 'CFSR' in self.registers and self.registers['CFSR'] & 0x00008000 else " - NOT VALID"
            print(f"BFAR  : 0x{bfar:08X} (BusFault Address Register{valid})")
            
        if 'AFSR' in self.registers:
            print(f"AFSR  : 0x{self.registers['AFSR']:08X} (Auxiliary Fault Status Register)")
        
        # Analyze fault
        print("\nFAULT ANALYSIS:")
        print("-" * 40)
        if 'CFSR' in self.registers and 'HFSR' in self.registers:
            cfsr = self.registers['CFSR']
            hfsr = self.registers['HFSR']
            
            # Determine primary fault
            primary_fault = "Unknown"
            fault_details = []
            
            if (cfsr >> 16) & 0xFFFF:  # UsageFault
                primary_fault = "UsageFault"
                if cfsr & 0x02000000:
                    fault_details.append("DIVBYZERO - Division by zero")
                    fault_details.append("  Check: DIV_0_TRP must be set in CCR for this trap")
                if cfsr & 0x01000000:
                    fault_details.append("UNALIGNED - Unaligned memory access")
                    fault_details.append("  - 32-bit access must be 4-byte aligned")
                    fault_details.append("  - 16-bit access must be 2-byte aligned")
                    fault_details.append("  Check: UNALIGN_TRP in CCR")
                    
            elif (cfsr >> 8) & 0xFF:  # BusFault
                primary_fault = "BusFault"
                if cfsr & 0x00000200:
                    fault_details.append("PRECISERR - Precise data bus error")
                    if cfsr & 0x00008000:
                        fault_details.append(f"  Fault address: 0x{self.registers.get('BFAR', 0):08X}")
                        
            elif cfsr & 0xFF:  # MemManage
                primary_fault = "MemManage Fault"
                if cfsr & 0x00000002:
                    fault_details.append("DACCVIOL - Data access violation")
                    if cfsr & 0x00000080:
                        fault_details.append(f"  Fault address: 0x{self.registers.get('MMFAR', 0):08X}")
            
            print(f"Primary Fault: {primary_fault}")
            for detail in fault_details:
                print(f"  {detail}")
                
            if hfsr & 0x40000000:
                print("\nEscalation: Fault escalated to HardFault")
                print("  The fault escalated because:")
                print("  - Fault handler is not enabled (check SHCSR)")
                print("  - Fault handler priority is misconfigured")
                print("  - A fault occurred inside the fault handler")
        
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
            total_size = 0x0f91bc  # End of last region (BACKUP_SRAM end)
            
            # Create buffer filled with 0xFF (unprogrammed flash pattern)
            dump_data = bytearray(b'\xFF' * total_size)
            
            # Map our regions to GDB script expectations
            region_mapping = {
                'DTCM': {'start': 0x20000000, 'file_offset': 0x0001bc, 'size': 0x20000},
                'AXI_SRAM': {'start': 0x24000000, 'file_offset': 0x0201bc, 'size': 0x80000},
                'SRAM1': {'start': 0x30000000, 'file_offset': 0x0a01bc, 'size': 0x20000},
                'SRAM2': {'start': 0x30020000, 'file_offset': 0x0c01bc, 'size': 0x20000},
                'SRAM3': {'start': 0x30040000, 'file_offset': 0x0e01bc, 'size': 0x08000},
                'SRAM4': {'start': 0x38000000, 'file_offset': 0x0e81bc, 'size': 0x10000},
                'BACKUP_SRAM': {'start': 0x38800000, 'file_offset': 0x0f81bc, 'size': 0x1000},
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
            
            # Write complete prstatus structure at offset 0x114
            reg_offset = 0x114
            
            # Create complete prstatus structure (148 bytes)
            prstatus = bytearray(148)
            
            # Fill prstatus header
            # pr_info (12 bytes) - signal info
            struct.pack_into('<i', prstatus, 0, 11)     # SIGSEGV
            struct.pack_into('<i', prstatus, 4, 0)      # si_code
            struct.pack_into('<i', prstatus, 8, 0)      # si_errno
            
            # pr_cursig (2 bytes) + padding
            struct.pack_into('<h', prstatus, 12, 11)    # SIGSEGV
            struct.pack_into('<h', prstatus, 14, 0)     # padding
            
            # pr_sigpend, pr_sighold (8 bytes)
            struct.pack_into('<II', prstatus, 16, 0, 0)
            
            # pr_pid, pr_ppid, pr_pgrp, pr_sid (16 bytes)
            struct.pack_into('<IIII', prstatus, 24, 1, 0, 1, 1)
            
            # pr_utime, pr_stime, pr_cutime, pr_cstime (32 bytes) - all zero
            # Already zero from bytearray initialization
            
            # pr_reg - registers start at offset 72
            reg_pos = 72
            for i in range(13):
                reg_name = f'R{i}'
                value = self.registers.get(reg_name, 0) & 0xFFFFFFFF
                struct.pack_into('<I', prstatus, reg_pos + i*4, value)
            
            # Special registers
            struct.pack_into('<I', prstatus, reg_pos + 13*4, self.registers.get('SP', 0) & 0xFFFFFFFF)
            struct.pack_into('<I', prstatus, reg_pos + 14*4, self.registers.get('LR', 0) & 0xFFFFFFFF)
            struct.pack_into('<I', prstatus, reg_pos + 15*4, self.registers.get('PC', 0) & 0xFFFFFFFF)
            struct.pack_into('<I', prstatus, reg_pos + 16*4, self.registers.get('PSR', 0) & 0xFFFFFFFF)
            struct.pack_into('<I', prstatus, reg_pos + 17*4, 0)  # orig_r0
            
            # pr_fpvalid (4 bytes)
            struct.pack_into('<I', prstatus, 144, 0)
            
            # Write prstatus at expected offset
            dump_data[reg_offset:reg_offset + len(prstatus)] = prstatus
            
            # Write MSP and PSP after prstatus for easy access
            # These are the actual stack pointers from the crash
            msp_psp_offset = reg_offset + len(prstatus)
            struct.pack_into('<I', dump_data, msp_psp_offset, self.registers.get('MSP', 0) & 0xFFFFFFFF)
            struct.pack_into('<I', dump_data, msp_psp_offset + 4, self.registers.get('PSP', 0) & 0xFFFFFFFF)
            
            # Write to file
            f.write(dump_data)
            
        print(f"Binary dump created: {os.path.getsize(filename)} bytes")
    
    def create_gdb_script(self, filename, bin_filename):
        """Create GDB script to load crash dump into live target"""
        print(f"\nCreating GDB script: {filename}")
        
        with open(filename, 'w') as f:
            # Header
            f.write(f"# GDB script to load crash dump from {datetime.now()}\n")
            f.write(f"# Generated by STM32H7 Crash Dump Server\n\n")
            
            # Set architecture
            f.write("# Set architecture\n")
            f.write("set architecture armv7e-m\n")
            f.write("set arm fallback-mode thumb\n")
            f.write("set arm force-mode thumb\n\n")
            
            # Restore registers
            f.write("# Restore CPU registers\n")
            f.write("echo \\n=== Restoring CPU registers ===\\n\n")
            
            # Write all registers
            for i in range(13):
                reg_name = f'R{i}'
                value = self.registers.get(reg_name, 0) & 0xFFFFFFFF
                f.write(f"set $r{i} = 0x{value:08X}\n")
            
            # Special registers
            f.write(f"set $sp = 0x{self.registers.get('SP', 0) & 0xFFFFFFFF:08X}\n")
            f.write(f"set $lr = 0x{self.registers.get('LR', 0) & 0xFFFFFFFF:08X}\n")
            f.write(f"set $pc = 0x{self.registers.get('PC', 0) & 0xFFFFFFFF:08X}\n")
            f.write(f"set $xpsr = 0x{self.registers.get('PSR', 0) & 0xFFFFFFFF:08X}\n")
            f.write(f"set $msp = 0x{self.registers.get('MSP', 0) & 0xFFFFFFFF:08X}\n")
            f.write(f"set $psp = 0x{self.registers.get('PSP', 0) & 0xFFFFFFFF:08X}\n")
            
            # Control registers
            f.write(f"set $control = 0x{self.registers.get('CONTROL', 0) & 0xFFFFFFFF:08X}\n")
            f.write(f"set $basepri = 0x{self.registers.get('BASEPRI', 0) & 0xFFFFFFFF:08X}\n")
            f.write(f"set $primask = 0x{self.registers.get('PRIMASK', 0) & 0xFFFFFFFF:08X}\n")
            f.write(f"set $faultmask = 0x{self.registers.get('FAULTMASK', 0) & 0xFFFFFFFF:08X}\n")
            f.write(f"set $fpscr = 0x{self.registers.get('FPSCR', 0) & 0xFFFFFFFF:08X}\n\n")
            
            # Memory regions (optional - may fail if target memory is protected)
            f.write("# Enable all STM32H7 power domains first\n")
            f.write("echo Enabling STM32H7 power domains...\\n\n")
            f.write("echo Enabling SRAM1/2/3 clocks (RCC AHB2ENR)...\\n\n")
            f.write("set *(unsigned int*)0x580244DC = *(unsigned int*)0x580244DC | 0xE0000000\n")
            f.write("echo Enabling SRAM4 and Backup SRAM clocks (RCC AHB4ENR)...\\n\n")
            f.write("set *(unsigned int*)0x580244E0 = *(unsigned int*)0x580244E0 | 0x10000000\n")
            f.write("echo Enabling backup domain access (PWR CR1)...\\n\n")
            f.write("set *(unsigned int*)0x58024800 = *(unsigned int*)0x58024800 | 0x00000100\n\n")
            
            f.write("# Restore memory regions (may fail if target memory is protected)\n")
            f.write("echo \\n=== Restoring memory regions ===\\n\n")
            
            # Map regions to offsets as in binary dump
            region_mapping = {
                'DTCM': {'addr': 0x20000000, 'start': 0x0001bc, 'end': 0x0201bc},
                'AXI_SRAM': {'addr': 0x24000000, 'start': 0x0201bc, 'end': 0x0a01bc},
                'SRAM1': {'addr': 0x30000000, 'start': 0x0a01bc, 'end': 0x0c01bc},
                'SRAM2': {'addr': 0x30020000, 'start': 0x0c01bc, 'end': 0x0e01bc},
                'SRAM3': {'addr': 0x30040000, 'start': 0x0e01bc, 'end': 0x0e81bc},
                'SRAM4': {'addr': 0x38000000, 'start': 0x0e81bc, 'end': 0x0f81bc},
                'BACKUP_SRAM': {'addr': 0x38800000, 'start': 0x0f81bc, 'end': 0x0f91bc},
            }
            
            for region_name, info in region_mapping.items():
                f.write(f"echo Restoring {region_name}...\\n\n")
                # The restore command syntax is: restore file binary BIAS start end
                # BIAS is SUBTRACTED from the destination address
                # So to load at addr from file offset start, BIAS = addr - start
                bias = info['addr'] - info['start']
                # Ensure bias is positive for formatting
                bias_formatted = bias & 0xFFFFFFFF
                f.write(f"restore {bin_filename} binary 0x{bias_formatted:08x} 0x{info['start']:x} 0x{info['end']:x}\n\n")
            
            # Show final state
            f.write("# Show restored state\n")
            f.write("echo \\n=== State restored! ===\\n\n")
            f.write("printf \"PC = 0x%08x  \", $pc\n")
            f.write("x/i $pc\n")
            f.write("printf \"LR = 0x%08x  \", $lr\n")
            f.write("x/i $lr\n")
            f.write("printf \"SP = 0x%08x\\n\", $sp\n\n")
            
            # Show registers
            f.write("echo \\n=== CPU Registers ===\\n\n")
            f.write("info registers\n\n")
            
            # Note: We don't adjust SP here because the crash handler already saved it correctly
            # The SP in the dump already points to the right location for GDB backtrace
            
            # Show backtrace
            f.write("# Show backtrace\n")
            f.write("echo \\n=== Backtrace ===\\n\n")
            f.write("bt\n\n")
            
            # Show code around crash
            f.write("# Show code around crash location\n")
            f.write("echo \\n=== Code at crash location ===\\n\n")
            f.write("list *$pc\n\n")
            
            # Force GUI update (for IDEs like CLion)
            f.write("# Update IDE view to show crash location\n")
            
            f.write("# Show all threads (OpenOCD RTOS support will handle this)\n")
            f.write("echo \\n=== All Threads ===\\n\n")
            f.write("info threads\n\n")  # List all threads
            
            f.write("# Show COMPLETE backtrace for ALL threads\n")
            f.write("echo \\n=== Complete Thread Backtraces ===\\n\n")
            f.write("thread apply all bt\n\n")  # FULL backtrace for ALL threads
            
            # Don't select a specific thread - use the current thread
            # GDB will already be on the crashed thread after loading PC
            
            # Analyze SCB Configuration from dump memory
            f.write("\n# Read System Control Block configuration from dump\n")
            f.write("echo \\n=== System Configuration (from dump) ===\\n\n")
            
            # CCR - Configuration Control Register
            f.write("# Configuration Control Register (CCR) at 0xE000ED14\n")
            f.write("set $ccr = *(unsigned int*)0xE000ED14\n")
            f.write("printf \"CCR = 0x%08x\\n\", $ccr\n")
            f.write("printf \"  [9]  STKALIGN   = %d : %s\\n\", ($ccr >> 9) & 1, ($ccr & 0x200) ? \"8-byte stack alignment\" : \"4-byte stack alignment\"\n")
            f.write("printf \"  [8]  BFHFNMIGN  = %d : %s\\n\", ($ccr >> 8) & 1, ($ccr & 0x100) ? \"Ignore BusFault in NMI/HardFault\" : \"Fault handlers enabled\"\n")
            f.write("printf \"  [4]  DIV_0_TRP  = %d : %s\\n\", ($ccr >> 4) & 1, ($ccr & 0x10) ? \"Divide by zero trap ENABLED\" : \"Divide by zero trap DISABLED\"\n")
            f.write("printf \"  [3]  UNALIGN_TRP= %d : %s\\n\", ($ccr >> 3) & 1, ($ccr & 0x08) ? \"Unaligned access trap ENABLED\" : \"Unaligned access trap DISABLED\"\n")
            f.write("printf \"  [1]  NONBASETHRDENA = %d : %s\\n\", ($ccr >> 1) & 1, ($ccr & 0x02) ? \"Thread mode can use PSP\" : \"Thread mode uses MSP only\"\n")
            f.write("\n")
            
            # SHCSR - System Handler Control and State Register
            f.write("# System Handler Control and State Register (SHCSR) at 0xE000ED24\n")
            f.write("set $shcsr = *(unsigned int*)0xE000ED24\n")
            f.write("printf \"SHCSR = 0x%08x\\n\", $shcsr\n")
            f.write("printf \"  [18] USGFAULTENA = %d : UsageFault handler %s\\n\", ($shcsr >> 18) & 1, ($shcsr & 0x40000) ? \"ENABLED\" : \"DISABLED\"\n")
            f.write("printf \"  [17] BUSFAULTENA = %d : BusFault handler %s\\n\", ($shcsr >> 17) & 1, ($shcsr & 0x20000) ? \"ENABLED\" : \"DISABLED\"\n")
            f.write("printf \"  [16] MEMFAULTENA = %d : MemManage handler %s\\n\", ($shcsr >> 16) & 1, ($shcsr & 0x10000) ? \"ENABLED\" : \"DISABLED\"\n")
            f.write("\n")
            
            # Check if all handlers are disabled
            f.write("if (($shcsr & 0x70000) == 0)\n")
            f.write("  echo ** WARNING: All configurable fault handlers are DISABLED!\\n\n")
            f.write("  echo ** All faults will escalate to HardFault!\\n\\n\n")
            f.write("end\n\n")
            
            # Analyze fault registers
            f.write("# Fault Status Registers\n")
            f.write("echo \\n=== Fault Status Registers ===\\n\n")
            
            # Use the fault register values from the crash dump
            if 'CFSR' in self.registers:
                f.write(f"set $cfsr = 0x{self.registers.get('CFSR', 0) & 0xFFFFFFFF:08X}\n")
            else:
                f.write("set $cfsr = *(unsigned int*)0xE000ED28\n")
                
            if 'HFSR' in self.registers:
                f.write(f"set $hfsr = 0x{self.registers.get('HFSR', 0) & 0xFFFFFFFF:08X}\n")
            else:
                f.write("set $hfsr = *(unsigned int*)0xE000ED2C\n")
                
            f.write("printf \"CFSR = 0x%08x\\n\", $cfsr\n")
            f.write("printf \"HFSR = 0x%08x\\n\", $hfsr\n")
            
            # Also read MMFAR and BFAR
            if 'MMFAR' in self.registers:
                f.write(f"set $mmfar = 0x{self.registers.get('MMFAR', 0) & 0xFFFFFFFF:08X}\n")
            else:
                f.write("set $mmfar = *(unsigned int*)0xE000ED34\n")
                
            if 'BFAR' in self.registers:
                f.write(f"set $bfar = 0x{self.registers.get('BFAR', 0) & 0xFFFFFFFF:08X}\n")
            else:
                f.write("set $bfar = *(unsigned int*)0xE000ED38\n")
                
            f.write("printf \"MMFAR = 0x%08x (MemManage Fault Address Register)\\n\", $mmfar\n")
            f.write("printf \"BFAR  = 0x%08x (BusFault Address Register)\\n\", $bfar\n\n")
            
            # Decode HFSR
            f.write("# Decode HardFault Status Register\n")
            f.write("echo \\n=== HardFault Status ===\\n\n")
            f.write("printf \"HFSR = 0x%08x (HardFault Status Register)\\n\", $hfsr\n")
            f.write("printf \"  [31] DEBUGEVT  = %d : %s\\n\", ($hfsr >> 31) & 1, ($hfsr & 0x80000000) ? \"Debug event occurred\" : \"No debug event\"\n")
            f.write("printf \"  [30] FORCED    = %d : %s\\n\", ($hfsr >> 30) & 1, ($hfsr & 0x40000000) ? \"Fault escalated to HardFault\" : \"Direct HardFault\"\n")
            f.write("printf \"  [1]  VECTTBL   = %d : %s\\n\", ($hfsr >> 1) & 1, ($hfsr & 0x00000002) ? \"Vector table read fault\" : \"No vector table fault\"\n\n")
            
            # Decode CFSR
            f.write("# Decode Configurable Fault Status Register\n")
            f.write("echo \\n=== CFSR Detailed Analysis ===\\n\n")
            f.write("printf \"CFSR = 0x%08x (Configurable Fault Status Register)\\n\", $cfsr\n")
            f.write("set $ufsr = ($cfsr >> 16) & 0xFFFF\n")
            f.write("set $bfsr = ($cfsr >> 8) & 0xFF\n")
            f.write("set $mmfsr = $cfsr & 0xFF\n")
            f.write("printf \"  UsageFault  (bits 31:16) = 0x%04x\\n\", $ufsr\n")
            f.write("printf \"  BusFault    (bits 15:8)  = 0x%02x\\n\", $bfsr\n")
            f.write("printf \"  MemManage   (bits 7:0)   = 0x%02x\\n\\n\", $mmfsr\n")
            # Check if fault address registers are valid and use dump values
            f.write("if ($cfsr & 0x00008000)\n")
            if 'BFAR' in self.registers:
                f.write(f"  printf \"- BFAR valid: 0x%08x\\n\", 0x{self.registers.get('BFAR', 0) & 0xFFFFFFFF:08X}\n")
            else:
                f.write("  printf \"- BFAR valid: 0x%08x\\n\", *(unsigned int*)0xE000ED38\n")
            f.write("end\n")
            
            f.write("if ($cfsr & 0x00000080)\n")
            if 'MMFAR' in self.registers:
                f.write(f"  printf \"- MMFAR valid: 0x%08x\\n\", 0x{self.registers.get('MMFAR', 0) & 0xFFFFFFFF:08X}\n")
            else:
                f.write("  printf \"- MMFAR valid: 0x%08x\\n\", *(unsigned int*)0xE000ED34\n")
            f.write("end\n")
            
            # Decode all CFSR bits according to ARM documentation
            f.write("\n# Decode CFSR fault bits\n")
            f.write("set $ufsr = ($cfsr >> 16) & 0xFFFF\n")
            f.write("set $bfsr = ($cfsr >> 8) & 0xFF\n")
            f.write("set $mmfsr = $cfsr & 0xFF\n\n")
            
            # UsageFault decoding
            f.write("if $ufsr\n")
            f.write("  echo \\n--- UsageFault Status ---\\n\n")
            f.write("  if ($cfsr & 0x02000000)\n")
            f.write("    echo - DIVBYZERO: Division by zero occurred\\n\n")
            f.write("  end\n")
            f.write("  if ($cfsr & 0x01000000)\n")
            f.write("    echo - UNALIGNED: Unaligned memory access\\n\n")
            f.write("  end\n")
            f.write("  if ($cfsr & 0x00080000)\n")
            f.write("    echo - NOCP: Attempted to access a coprocessor\\n\n")
            f.write("  end\n")
            f.write("  if ($cfsr & 0x00040000)\n")
            f.write("    echo - INVPC: Invalid PC load (illegal EXC_RETURN)\\n\n")
            f.write("  end\n")
            f.write("  if ($cfsr & 0x00020000)\n")
            f.write("    echo - INVSTATE: Invalid state (EPSR.T bit issue)\\n\n")
            f.write("  end\n")
            f.write("  if ($cfsr & 0x00010000)\n")
            f.write("    echo - UNDEFINSTR: Undefined instruction executed\\n\n")
            f.write("  end\n")
            f.write("end\n\n")
            
            # BusFault decoding
            f.write("if $bfsr\n")
            f.write("  echo \\n--- BusFault Status ---\\n\n")
            f.write("  if ($cfsr & 0x00002000)\n")
            f.write("    echo - LSPERR: Bus fault on floating-point lazy state preservation\\n\n")
            f.write("  end\n")
            f.write("  if ($cfsr & 0x00001000)\n")
            f.write("    echo - STKERR: Bus fault on exception stacking\\n\n")
            f.write("  end\n")
            f.write("  if ($cfsr & 0x00000800)\n")
            f.write("    echo - UNSTKERR: Bus fault on exception unstacking\\n\n")
            f.write("  end\n")
            f.write("  if ($cfsr & 0x00000400)\n")
            f.write("    echo - IMPRECISERR: Imprecise data bus error\\n\n")
            f.write("  end\n")
            f.write("  if ($cfsr & 0x00000200)\n")
            f.write("    echo - PRECISERR: Precise data bus error\\n\n")
            f.write("  end\n")
            f.write("  if ($cfsr & 0x00000100)\n")
            f.write("    echo - IBUSERR: Instruction bus error\\n\n")
            f.write("  end\n")
            f.write("end\n\n")
            
            # MemManage Fault decoding
            f.write("if $mmfsr\n")
            f.write("  echo \\n--- MemManage Status ---\\n\n")
            f.write("  if ($cfsr & 0x00000020)\n")
            f.write("    echo - MLSPERR: MemManage fault on floating-point lazy state preservation\\n\n")
            f.write("  end\n")
            f.write("  if ($cfsr & 0x00000010)\n")
            f.write("    echo - MSTKERR: MemManage fault on exception stacking\\n\n")
            f.write("  end\n")
            f.write("  if ($cfsr & 0x00000008)\n")
            f.write("    echo - MUNSTKERR: MemManage fault on exception unstacking\\n\n")
            f.write("  end\n")
            f.write("  if ($cfsr & 0x00000002)\n")
            f.write("    echo - DACCVIOL: Data access violation\\n\n")
            f.write("  end\n")
            f.write("  if ($cfsr & 0x00000001)\n")
            f.write("    echo - IACCVIOL: Instruction access violation\\n\n")
            f.write("  end\n")
            f.write("end\n\n")
            
            # Add fault analysis summary
            f.write("\n# Fault Analysis Summary\n")
            f.write("echo \\n=== Fault Analysis Summary ===\\n\n")
            
            # Check primary fault type
            f.write("if $ufsr\n")
            f.write("  echo Primary Fault: UsageFault\\n\n")
            f.write("  if ($cfsr & 0x01000000)\n")
            f.write("    echo   - UNALIGNED access detected\\n\n")
            f.write("    if (($ccr & 0x08) == 0)\n")
            f.write("      echo   - WARNING: UNALIGN_TRP is DISABLED but fault occurred!\\n\n")
            f.write("    end\n")
            f.write("  end\n")
            f.write("  if ($cfsr & 0x02000000)\n")
            f.write("    echo   - DIVBYZERO detected\\n\n")
            f.write("    if (($ccr & 0x10) == 0)\n")
            f.write("      echo   - WARNING: DIV_0_TRP is DISABLED but fault occurred!\\n\n")
            f.write("    end\n")
            f.write("  end\n")
            f.write("end\n")
            
            f.write("if $bfsr && !$ufsr\n")
            f.write("  echo Primary Fault: BusFault\\n\n")
            f.write("end\n")
            
            f.write("if $mmfsr && !$ufsr && !$bfsr\n")
            f.write("  echo Primary Fault: MemManage Fault\\n\n")
            f.write("end\n")
            
            # Check escalation
            f.write("if ($hfsr & 0x40000000)\n")
            f.write("  echo Escalation to HardFault detected!\\n\n")
            f.write("  echo Possible reasons:\\n\n")
            f.write("  if $ufsr && (($shcsr & 0x40000) == 0)\n")
            f.write("    echo   - UsageFault handler DISABLED (USGFAULTENA=0)\\n\n")
            f.write("  end\n")
            f.write("  if $bfsr && (($shcsr & 0x20000) == 0)\n")
            f.write("    echo   - BusFault handler DISABLED (BUSFAULTENA=0)\\n\n")
            f.write("  end\n")
            f.write("  if $mmfsr && (($shcsr & 0x10000) == 0)\n")
            f.write("    echo   - MemManage handler DISABLED (MEMFAULTENA=0)\\n\n")
            f.write("  end\n")
            f.write("end\n\n")
            
            # Final commands to position at crash
            f.write("# Position at crash location\n")
            f.write("echo \\n=== Positioning at crash location ===\\n\n")
            
            # Refresh thread and debug views
            f.write("# Refresh debugger views\n")
            f.write("info threads\n")  # Update thread list
            f.write("info locals\n")   # Update local variables
            f.write("info args\n")     # Update function arguments
            
            # Force thread detection without side effects
            f.write("\n# Force thread detection without side effects\n")
            f.write("echo \\n=== Forcing thread detection ===\\n\n")
            
            # Save PC before stepping
            f.write("# Save PC and step\n")
            f.write("set $saved_pc = $pc\n")
            f.write("stepi\n")
            
            # Restore PC to original crash location
            f.write("# Restore PC to crash point\n")
            f.write("set $pc = $saved_pc\n\n")
            
            f.write("echo Thread detection complete - back at crash point\\n\n")
            f.write("info threads\n")
            
            # Check if we're in an exception handler by looking at LR
            f.write("\n# Check if we're in an exception handler\n")
            f.write("# ARM Cortex-M uses special EXC_RETURN values in LR\n")
            f.write("if ($lr & 0xFFFFFFF0) == 0xFFFFFFE0 || ($lr & 0xFFFFFFF0) == 0xFFFFFFF0\n")
            f.write("  printf \"=== Exception return detected (LR = 0x%08x) ===\\n\\n\", $lr\n")
            f.write("  echo GDB should handle exception unwinding automatically\\n\\n")
            f.write("  echo \\nPerforming backtrace...\\n\n")
            f.write("  bt\n")
            f.write("  echo \\n\n")
            f.write("else\n")
            f.write("  # Not in exception handler, normal backtrace\n")
            f.write("  echo Normal execution context\\n\n")
            f.write("  bt\n")
            f.write("end\n\n")
            
            # Final status
            f.write("\necho \\n=== Crash dump loaded successfully! ===\\n\n")
            f.write("echo You are now at the exact point of the crash.\\n\n")
            f.write("echo Use 'bt' for backtrace, 'info locals' for variables.\\n\n")
            
        print(f"GDB script created: {os.path.getsize(filename)} bytes")

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