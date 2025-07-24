echo \n=== Testing STM32H7 Memory Access ===\n\n

echo Testing DTCM @ 0x20000000... \n
set *(int*)0x20000000 = 0x12345678
set $test = *(int*)0x20000000
if ($test == 0x12345678)
  echo OK\n
else
  echo FAILED\n
end

echo Testing AXI SRAM @ 0x24000000... \n
set *(int*)0x24000000 = 0x12345678
set $test = *(int*)0x24000000
if ($test == 0x12345678)
  echo OK\n
else
  echo FAILED\n
end

echo Testing SRAM1 @ 0x30000000... \n
set *(int*)0x30000000 = 0x12345678
set $test = *(int*)0x30000000
if ($test == 0x12345678)
  echo OK\n
else
  echo FAILED\n
end

echo Testing SRAM2 @ 0x30020000... \n
set *(int*)0x30020000 = 0x12345678
set $test = *(int*)0x30020000
if ($test == 0x12345678)
  echo OK\n
else
  echo FAILED\n
end

echo Testing SRAM3 @ 0x30040000... \n
set *(int*)0x30040000 = 0x12345678
set $test = *(int*)0x30040000
if ($test == 0x12345678)
  echo OK\n
else
  echo FAILED\n
end

echo Testing SRAM4 @ 0x38000000... \n
set *(int*)0x38000000 = 0x12345678
set $test = *(int*)0x38000000
if ($test == 0x12345678)
  echo OK\n
else
  echo FAILED\n
end

echo Testing Backup SRAM @ 0x38800000... \n
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