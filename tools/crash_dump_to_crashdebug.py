#!/usr/bin/env python3
"""
TCP Server that receives STM32H7 crash dumps and creates CrashDebug-compatible dump files
CrashDebug is the standard tool for debugging Cortex-M crashes with GDB
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
            print("Will create CrashDebug-compatible dump files")
            
            while True:
                conn, addr = server.accept()
                print(f"\nConnection from {addr}")
                
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
                                    print(f"Receiving region: {region_name}")
                                elif current_region and ':' in line:
                                    # Parse memory line
                                    addr, data = self.parse_hexdump_line(line)
                                    if addr is not None and data:
                                        current_region['data'][addr] = data
                        
                except Exception as e:
                    print(f"Error receiving dump: {e}")
                finally:
                    conn.close()
                    
                # Generate CrashDebug files if we got data
                if self.registers and self.memory_regions:
                    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                    
                    self.print_crash_summary()
                    
                    # Create CrashDebug dump file
                    dump_filename = f"crashdump_{timestamp}.dump"
                    self.create_crashdebug_dump(dump_filename)
                    
                    # Create memory binary file
                    mem_filename = f"memory_{timestamp}.bin"
                    self.create_memory_binary(mem_filename)
                    
                    print(f"\n[SUCCESS] CrashDebug files created:")
                    print(f"  - Dump file: {dump_filename}")
                    print(f"  - Memory file: {mem_filename}")
                    print(f"\nTo debug:")
                    print(f"1. Get CrashDebug from: https://github.com/adamgreen/CrashDebug")
                    print(f"2. Run: arm-none-eabi-gdb your_app.elf")
                    print(f"3. In GDB: target remote | CrashDebug --dump {dump_filename} --elf your_app.elf")
                    print(f"4. In GDB: bt")
                    print(f"5. In GDB: list")
                    print(f"6. In GDB: info registers")
                    
    def print_crash_summary(self):
        """Print summary of crash information"""
        print("\n" + "="*60)
        print("CRASH DUMP ANALYSIS")
        print("="*60)
        
        # Print key registers
        print("\nCPU STATE AT CRASH:")
        print("-" * 40)
        if 'PC' in self.registers:
            print(f"PC (crash location): 0x{self.registers['PC']:08X}")
        if 'LR' in self.registers:
            print(f"LR (return address): 0x{self.registers['LR']:08X}")
        if 'SP' in self.registers:
            print(f"SP (stack pointer):  0x{self.registers['SP']:08X}")
        
        # Decode fault registers
        if 'CFSR' in self.registers:
            cfsr = self.registers['CFSR']
            print(f"\nCFSR: 0x{cfsr:08X}")
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
        
        # Memory summary
        print("\nMEMORY CAPTURED:")
        print("-" * 40)
        total_size = 0
        for region in self.memory_regions:
            size = sum(len(data) for data in region['data'].values())
            total_size += size
            print(f"{region['name']:12}: {size:8} bytes")
        print(f"{'TOTAL':12}: {total_size:8} bytes ({total_size/1024:.1f} KB)")
        print("="*60)
        
    def create_crashdebug_dump(self, filename):
        """Create CrashDebug format dump file"""
        with open(filename, 'w') as f:
            # Write register values in CrashDebug format
            f.write("Registers:\n")
            
            # General purpose registers
            for i in range(13):
                reg_name = f'R{i}'
                value = self.registers.get(reg_name, 0)
                f.write(f"R{i} = 0x{value:08X}\n")
            
            # Special registers
            f.write(f"SP = 0x{self.registers.get('SP', 0):08X}\n")
            f.write(f"LR = 0x{self.registers.get('LR', 0):08X}\n")
            f.write(f"PC = 0x{self.registers.get('PC', 0):08X}\n")
            f.write(f"xPSR = 0x{self.registers.get('PSR', 0):08X}\n")
            
            # MSP/PSP if available
            if 'MSP' in self.registers:
                f.write(f"MSP = 0x{self.registers['MSP']:08X}\n")
            if 'PSP' in self.registers:
                f.write(f"PSP = 0x{self.registers['PSP']:08X}\n")
            
            # Fault registers
            f.write("\nFault Status Registers:\n")
            if 'HFSR' in self.registers:
                f.write(f"HFSR = 0x{self.registers['HFSR']:08X}\n")
            if 'CFSR' in self.registers:
                f.write(f"CFSR = 0x{self.registers['CFSR']:08X}\n")
            if 'MMFAR' in self.registers:
                f.write(f"MMFAR = 0x{self.registers['MMFAR']:08X}\n")
            if 'BFAR' in self.registers:
                f.write(f"BFAR = 0x{self.registers['BFAR']:08X}\n")
            if 'AFSR' in self.registers:
                f.write(f"AFSR = 0x{self.registers['AFSR']:08X}\n")
            
            # Memory regions
            f.write("\nMemory Regions:\n")
            for region in self.memory_regions:
                f.write(f"\n{region['name']}:\n")
                # Sort addresses for consistent output
                for addr in sorted(region['data'].keys()):
                    data = region['data'][addr]
                    # Write in CrashDebug hex format
                    hex_str = ' '.join(f"{b:02X}" for b in data)
                    f.write(f"0x{addr:08X}: {hex_str}\n")
    
    def create_memory_binary(self, filename):
        """Create binary file with all memory contents"""
        with open(filename, 'wb') as f:
            # Write a simple header
            f.write(b'STMCRASH')  # Magic
            f.write(struct.pack('<I', len(self.memory_regions)))  # Region count
            
            # Write each region
            for region in self.memory_regions:
                # Region name (16 bytes)
                name = region['name'].encode('utf-8')[:16].ljust(16, b'\0')
                f.write(name)
                
                # Number of blocks in this region
                f.write(struct.pack('<I', len(region['data'])))
                
                # Write each memory block
                for addr in sorted(region['data'].keys()):
                    data = region['data'][addr]
                    f.write(struct.pack('<I', addr))       # Address
                    f.write(struct.pack('<I', len(data)))  # Length
                    f.write(data)                          # Data

def main():
    parser = argparse.ArgumentParser(
        description='STM32H7 Crash Dump Server - Creates CrashDebug format dumps'
    )
    parser.add_argument('--host', default='0.0.0.0', help='Host to listen on')
    parser.add_argument('--port', type=int, default=9999, help='Port to listen on')
    
    args = parser.parse_args()
    
    print("STM32H7 Crash Dump Server (CrashDebug format)")
    print("=" * 50)
    print(f"Listening on {args.host}:{args.port}")
    print("Creates dump files compatible with CrashDebug")
    print("Press Ctrl+C to stop\n")
    
    server = CrashDumpServer(args.host, args.port)
    try:
        server.receive_dump()
    except KeyboardInterrupt:
        print("\nServer stopped")

if __name__ == '__main__':
    main()