#!/usr/bin/env python3
"""
Test client to simulate STM32H7 crash dump
"""

import socket
import time

def send_test_dump(host='localhost', port=9999):
    """Send a test crash dump in hexdump format"""
    
    # Test crash data
    crash_header = "\r\n\r\nCRASH ENCOUNTERED\r\n"
    
    # Registers - matching actual STM32 format (8 hex digits, no 0x prefix)
    registers = [
        "R0: 20001234",
        "R1: 00000001", 
        "R2: DEADBEEF",
        "R3: 00000000",
        "R4: 00000000",
        "R5: 00000000",
        "R6: 00000000",
        "R7: 00000000",
        "R8: 00000000",
        "R9: 00000000",
        "R10: 00000000",
        "R11: 00000000",
        "R12: 00000000",
        "SP: 20010000",
        "LR: 08001234",
        "PC: 08005678",  # Where it crashed
        "PSR: 61000000",
        "HFSR: 40000000",  # Forced HardFault
        "CFSR: 00008200",  # BusFault + BFARVALID
        "MMFAR: 00000000",
        "BFAR: DEADBEEF",  # Bus fault address
        "AFSR: 00000000",
    ]
    
    # Memory dump header
    memory_header = "\r\nMemory dump:\r\n"
    
    # Small test memory regions
    memory_regions = [
        {
            'name': 'DTCM',
            'data': [
                "20000000: 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 10",
                "20000010: 11 12 13 14 15 16 17 18 19 1A 1B 1C 1D 1E 1F 20",
                "20000020: DE AD BE EF CA FE BA BE 00 00 00 00 00 00 00 00",
            ]
        },
        {
            'name': 'AXI_SRAM',
            'data': [
                "24000000: 48 65 6C 6C 6F 20 57 6F 72 6C 64 21 00 00 00 00",  # "Hello World!"
                "24000010: 54 68 69 73 20 69 73 20 61 20 74 65 73 74 00 00",  # "This is a test"
            ]
        }
    ]
    
    footer = "\r\nEnd of dump\r\n"
    
    # Connect and send
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.connect((host, port))
            print(f"Connected to {host}:{port}")
            
            # Send crash header
            sock.sendall(crash_header.encode())
            time.sleep(0.1)
            
            # Send registers
            for reg in registers:
                sock.sendall((reg + "\r\n").encode())
                time.sleep(0.01)
            
            # Send memory header
            sock.sendall(memory_header.encode())
            time.sleep(0.1)
            
            # Send memory regions
            for region in memory_regions:
                sock.sendall(f"\r\n{region['name']}:\r\n".encode())
                for line in region['data']:
                    sock.sendall((line + "\r\n").encode())
                    time.sleep(0.01)
            
            # Send footer
            sock.sendall(footer.encode())
            
            print("Test crash dump sent successfully")
            
    except Exception as e:
        print(f"Error: {e}")

if __name__ == '__main__':
    send_test_dump()