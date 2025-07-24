echo \n=== Enabling STM32H7 Power Domains ===\n\n

echo Enabling SRAM1/2/3 clocks...\n
set $ahb2enr = *(unsigned int*)0x580244DC
set $ahb2enr = $ahb2enr | 0xE0000000
set *(unsigned int*)0x580244DC = $ahb2enr
echo Done\n

echo Enabling SRAM4 and Backup SRAM clocks...\n
set $ahb4enr = *(unsigned int*)0x580244E0
set $ahb4enr = $ahb4enr | 0x10000000
set *(unsigned int*)0x580244E0 = $ahb4enr
echo Done\n

echo Enabling Backup domain access...\n
set $pwr_cr1 = *(unsigned int*)0x58024800
set $pwr_cr1 = $pwr_cr1 | 0x00000100
set *(unsigned int*)0x58024800 = $pwr_cr1
echo Done\n

shell sleep 0.1

echo \n=== Testing Memory Access After Enabling Domains ===\n
source openocd/test_memory_access.gdb