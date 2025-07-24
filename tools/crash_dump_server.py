#!/usr/bin/env python3
"""
TCP Server for receiving STM32H7 crash dumps and creating ELF files
Compatible with CrashCatcher format for debugging with CrashDebug
"""

import socket
import struct
import sys
import argparse
import os
from datetime import datetime
from elftools.elf.elffile import ELFFile
from elftools.elf.structs import ELFStructs

class CrashDumpServer:
    def __init__(self, host='0.0.0.0', port=9999):
        self.host = host
        self.port = port
        self.dump_data = bytearray()
        self.registers = {}
        self.memory_regions = []
        
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
                    
                # Generate ELF if we got data
                if self.registers and self.memory_regions:
                    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                    elf_filename = f"crash_dump_{timestamp}.elf"
                    self.create_elf(elf_filename)
                    print(f"\nELF file created: {elf_filename}")
                    print(f"Use with CrashDebug: arm-none-eabi-gdb -ex 'target remote | CrashDebug --elf {elf_filename}'")
                    
    def create_elf(self, filename):
        """Create ELF file compatible with CrashDebug"""
        # ELF header for ARM Cortex-M7
        e_ident = b'\x7fELF'  # Magic
        e_ident += b'\x01'    # 32-bit
        e_ident += b'\x01'    # Little endian
        e_ident += b'\x01'    # Current version
        e_ident += b'\x00' * 9  # Padding
        
        # ELF header fields
        e_type = 0x0002      # ET_EXEC
        e_machine = 0x0028   # EM_ARM
        e_version = 0x00000001
        e_entry = self.registers.get('PC', 0)
        e_phoff = 0x34       # Program header offset
        e_shoff = 0          # No section headers
        e_flags = 0x05000000 # ARM EABI
        e_ehsize = 0x34      # ELF header size
        e_phentsize = 0x20   # Program header entry size
        e_phnum = 0          # Will be calculated
        e_shentsize = 0
        e_shnum = 0
        e_shstrndx = 0
        
        # Build memory segments
        segments = []
        
        # Add register note segment (for CrashDebug)
        note_data = self.create_register_note()
        segments.append({
            'type': 0x04,  # PT_NOTE
            'offset': 0,
            'vaddr': 0,
            'paddr': 0,
            'filesz': len(note_data),
            'memsz': len(note_data),
            'flags': 0x04,  # PF_R
            'align': 4,
            'data': note_data
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
    
    def create_register_note(self):
        """Create PT_NOTE segment with register values for CrashDebug"""
        # Note name "CORE\0"
        name = b'CORE\0'
        while len(name) % 4:
            name += b'\0'
        
        # Build register data (matches CrashCatcher format)
        # Order: R0-R12, SP, LR, PC, PSR, then fault registers
        reg_data = bytearray()
        
        # General purpose registers
        for i in range(13):
            reg_name = f'R{i}'
            value = self.registers.get(reg_name, 0)
            reg_data.extend(struct.pack('<I', value))
        
        # SP, LR, PC, PSR
        for reg in ['SP', 'LR', 'PC', 'PSR']:
            value = self.registers.get(reg, 0)
            reg_data.extend(struct.pack('<I', value))
        
        # Fault registers
        for reg in ['HFSR', 'CFSR', 'MMFAR', 'BFAR', 'AFSR']:
            value = self.registers.get(reg, 0)
            reg_data.extend(struct.pack('<I', value))
        
        # Note header
        note = bytearray()
        note.extend(struct.pack('<I', len(name)))  # namesz
        note.extend(struct.pack('<I', len(reg_data)))  # descsz
        note.extend(struct.pack('<I', 0x01))  # NT_PRSTATUS
        note.extend(name)
        note.extend(reg_data)
        
        return note

def main():
    parser = argparse.ArgumentParser(description='STM32H7 Crash Dump Server')
    parser.add_argument('--host', default='0.0.0.0', help='Host to listen on')
    parser.add_argument('--port', type=int, default=9999, help='Port to listen on')
    
    args = parser.parse_args()
    
    server = CrashDumpServer(args.host, args.port)
    try:
        server.receive_dump()
    except KeyboardInterrupt:
        print("\nServer stopped")

if __name__ == '__main__':
    main()