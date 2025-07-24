#!/usr/bin/env python3
"""
TCP Server for receiving STM32H7 crash dumps and creating GDB-compatible core dumps
Specifically formatted for bare metal ARM Cortex-M targets
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
            value = int(parts[1].strip(), 16)
            return reg_name, value
        except:
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
                                    print(f"Register {reg_name}: 0x{value:08X}")
                            
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
                    
                    self.print_crash_summary()
                    self.create_minimal_core(core_filename)
                    
                    print(f"\n[SUCCESS] Core dump created: {core_filename}")
                    print(f"\nTo debug:")
                    print(f"1. arm-none-eabi-gdb your_app.elf")
                    print(f"2. (gdb) set arm fallback-mode thumb")
                    print(f"3. (gdb) target core {core_filename}")
                    print(f"4. (gdb) info registers")
                    print(f"5. (gdb) bt")
                    
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
        
    def create_minimal_core(self, filename):
        """Create minimal ELF core dump for bare metal ARM"""
        # ELF header
        elf_header = bytearray()
        elf_header.extend(b'\x7fELF')           # Magic
        elf_header.extend(b'\x01')              # 32-bit
        elf_header.extend(b'\x01')              # Little endian
        elf_header.extend(b'\x01')              # Current version
        elf_header.extend(b'\x00' * 9)          # Padding
        elf_header.extend(struct.pack('<H', 4)) # ET_CORE
        elf_header.extend(struct.pack('<H', 40)) # EM_ARM
        elf_header.extend(struct.pack('<I', 1))  # Version
        elf_header.extend(struct.pack('<I', 0))  # Entry (none for core)
        elf_header.extend(struct.pack('<I', 52)) # Program header offset
        elf_header.extend(struct.pack('<I', 0))  # Section header offset
        elf_header.extend(struct.pack('<I', 0x05000000)) # Flags
        elf_header.extend(struct.pack('<H', 52)) # ELF header size
        elf_header.extend(struct.pack('<H', 32)) # Program header size
        elf_header.extend(struct.pack('<H', 0))  # Program header count (updated later)
        elf_header.extend(struct.pack('<H', 0))  # Section header size
        elf_header.extend(struct.pack('<H', 0))  # Section header count
        elf_header.extend(struct.pack('<H', 0))  # String table index
        
        # Create register note
        reg_note = self.create_simple_reg_note()
        
        # Build segments
        segments = []
        
        # Note segment
        segments.append({
            'type': 4,  # PT_NOTE
            'offset': 0,
            'vaddr': 0,
            'paddr': 0,
            'filesz': len(reg_note),
            'memsz': len(reg_note),
            'flags': 4,  # PF_R
            'align': 4,
            'data': reg_note
        })
        
        # Memory segments
        for region in self.memory_regions:
            if region['data']:
                # Create continuous blocks
                addresses = sorted(region['data'].keys())
                if not addresses:
                    continue
                    
                start_addr = addresses[0]
                data = bytearray()
                expected_addr = start_addr
                
                for addr in addresses:
                    if addr != expected_addr:
                        # Gap - create segment
                        if data:
                            segments.append({
                                'type': 1,  # PT_LOAD
                                'offset': 0,
                                'vaddr': start_addr,
                                'paddr': start_addr,
                                'filesz': len(data),
                                'memsz': len(data),
                                'flags': 7,  # PF_R | PF_W | PF_X
                                'align': 4,
                                'data': data
                            })
                        start_addr = addr
                        data = bytearray()
                        expected_addr = addr
                    
                    data.extend(region['data'][addr])
                    expected_addr = addr + len(region['data'][addr])
                
                # Final segment
                if data:
                    segments.append({
                        'type': 1,  # PT_LOAD
                        'offset': 0,
                        'vaddr': start_addr,
                        'paddr': start_addr,
                        'filesz': len(data),
                        'memsz': len(data),
                        'flags': 7,  # PF_R | PF_W | PF_X
                        'align': 4,
                        'data': data
                    })
        
        # Calculate offsets
        current_offset = 52 + len(segments) * 32  # After headers
        for seg in segments:
            seg['offset'] = current_offset
            current_offset += seg['filesz']
            # Align to 4 bytes
            if current_offset % 4:
                current_offset += 4 - (current_offset % 4)
        
        # Update program header count
        struct.pack_into('<H', elf_header, 44, len(segments))
        
        # Write file
        with open(filename, 'wb') as f:
            f.write(elf_header)
            
            # Write program headers
            for seg in segments:
                f.write(struct.pack('<I', seg['type']))
                f.write(struct.pack('<I', seg['offset']))
                f.write(struct.pack('<I', seg['vaddr']))
                f.write(struct.pack('<I', seg['paddr']))
                f.write(struct.pack('<I', seg['filesz']))
                f.write(struct.pack('<I', seg['memsz']))
                f.write(struct.pack('<I', seg['flags']))
                f.write(struct.pack('<I', seg['align']))
            
            # Write segment data
            for seg in segments:
                f.seek(seg['offset'])
                f.write(seg['data'])
    
    def create_simple_reg_note(self):
        """Create simple register note that GDB can understand"""
        # For bare metal ARM, we'll create a custom note
        name = b'CORE\0'
        while len(name) % 4:
            name += b'\0'
        
        # Simple register dump in order GDB expects
        # Just the raw register values
        regs = bytearray()
        
        # R0-R15 + CPSR
        for i in range(13):
            regs.extend(struct.pack('<I', self.registers.get(f'R{i}', 0)))
        regs.extend(struct.pack('<I', self.registers.get('SP', 0)))  # R13
        regs.extend(struct.pack('<I', self.registers.get('LR', 0)))  # R14
        regs.extend(struct.pack('<I', self.registers.get('PC', 0)))  # R15
        regs.extend(struct.pack('<I', self.registers.get('PSR', 0))) # CPSR
        
        # Create note
        note = bytearray()
        note.extend(struct.pack('<I', len(name)))
        note.extend(struct.pack('<I', len(regs)))
        note.extend(struct.pack('<I', 1))  # NT_PRSTATUS
        note.extend(name)
        note.extend(regs)
        
        # Align to 4 bytes
        while len(note) % 4:
            note.append(0)
            
        return note

def main():
    parser = argparse.ArgumentParser(description='STM32H7 Crash Dump Server - GDB Core Dumps')
    parser.add_argument('--host', default='0.0.0.0', help='Host to listen on')
    parser.add_argument('--port', type=int, default=9999, help='Port to listen on')
    
    args = parser.parse_args()
    
    print(f"STM32H7 Crash Dump Server (GDB version)")
    print(f"Creates bare metal ARM core dumps")
    print(f"Listening on {args.host}:{args.port}")
    print(f"Press Ctrl+C to stop\n")
    
    server = CrashDumpServer(args.host, args.port)
    try:
        server.receive_dump()
    except KeyboardInterrupt:
        print("\nServer stopped")

if __name__ == '__main__':
    main()