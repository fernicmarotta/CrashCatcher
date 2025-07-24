# Test memory access for STM32H7
echo \n=== Testing STM32H7 Memory Access ===\n\n

# Test DTCM (0x20000000)
echo Testing DTCM @ 0x20000000... 
set *(int*)0x20000000 = 0x12345678
set $test = *(int*)0x20000000
if ($test == 0x12345678)
  echo OK\n
else
  echo FAILED\n
end

# Test AXI SRAM (0x24000000)
echo Testing AXI SRAM @ 0x24000000... 
set *(int*)0x24000000 = 0x12345678
set $test = *(int*)0x24000000
if ($test == 0x12345678)
  echo OK\n
else
  echo FAILED\n
end

# Test SRAM1 (0x30000000)
echo Testing SRAM1 @ 0x30000000... 
set *(int*)0x30000000 = 0x12345678
set $test = *(int*)0x30000000
if ($test == 0x12345678)
  echo OK\n
else
  echo FAILED\n
end

# Test SRAM2 (0x30020000)
echo Testing SRAM2 @ 0x30020000... 
set *(int*)0x30020000 = 0x12345678
set $test = *(int*)0x30020000
if ($test == 0x12345678)
  echo OK\n
else
  echo FAILED\n
end

# Test SRAM3 (0x30040000)
echo Testing SRAM3 @ 0x30040000... 
set *(int*)0x30040000 = 0x12345678
set $test = *(int*)0x30040000
if ($test == 0x12345678)
  echo OK\n
else
  echo FAILED\n
end

# Test SRAM4 (0x38000000)
echo Testing SRAM4 @ 0x38000000... 
set *(int*)0x38000000 = 0x12345678
set $test = *(int*)0x38000000
if ($test == 0x12345678)
  echo OK\n
else
  echo FAILED\n
end

# Test Backup SRAM (0x38800000)
echo Testing Backup SRAM @ 0x38800000... 
set *(int*)0x38800000 = 0x12345678
set $test = *(int*)0x38800000
if ($test == 0x12345678)
  echo OK\n
else
  echo FAILED\n
end

echo \n=== Memory Map Summary ===\n
maintenance info sections

echo \n=== Target Info ===\n
info target