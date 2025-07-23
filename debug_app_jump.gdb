# Debug script for bootloader to app jump

# Connect to target
target remote localhost:3333
monitor reset halt

# Load bootloader symbols (main)
file build/arm-none-eabi/Debug/CORTEX_COREDUMP_1.0.0.0.elf
load

# Add application symbols without loading
add-symbol-file /home/fnicolas/Documentos/GIT/nb_combiner/build/arm-none-eabi/Debug/NUBE_CB_APP_6.0.0.0.elf 0x08020000

# Set breakpoints
b Jump_To_Application
b jump_to_application_asm

# Breakpoint en SystemInit de la aplicación especificando el archivo exacto
b system_stm32h7xx.c:SystemInit

# También puedes poner breakpoint en el Reset_Handler de la app
b *0x08090bdc

# Run and see where we land
continue