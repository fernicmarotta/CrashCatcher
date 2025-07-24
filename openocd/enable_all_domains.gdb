# Enable all power domains on STM32H7
echo \n=== Enabling STM32H7 Power Domains ===\n\n

# Read RCC AHB2ENR to enable SRAM1/2/3
echo Enabling SRAM1/2/3 clocks...
set $ahb2enr = *(unsigned int*)0x580244DC
set $ahb2enr = $ahb2enr | 0xE0000000  # Enable SRAM1, SRAM2, SRAM3
set *(unsigned int*)0x580244DC = $ahb2enr
echo Done\n

# Read RCC AHB4ENR to enable SRAM4 and Backup SRAM
echo Enabling SRAM4 and Backup SRAM clocks...
set $ahb4enr = *(unsigned int*)0x580244E0
set $ahb4enr = $ahb4enr | 0x10000000  # Enable Backup SRAM
set *(unsigned int*)0x580244E0 = $ahb4enr
echo Done\n

# Enable backup domain access
echo Enabling Backup domain access...
# PWR CR1 - Enable backup domain write access
set $pwr_cr1 = *(unsigned int*)0x58024800
set $pwr_cr1 = $pwr_cr1 | 0x00000100  # DBP bit
set *(unsigned int*)0x58024800 = $pwr_cr1
echo Done\n

# Wait a bit for clocks to stabilize
shell sleep 0.1

# Test access again
echo \n=== Testing Memory Access After Enabling Domains ===\n
source openocd/test_memory_access.gdb