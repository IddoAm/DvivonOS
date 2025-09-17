# GDB script to debug bootloader memory
target remote 127.0.0.1:1234
set architecture i386

# Set breakpoints
break *0x7c00
break *0x7e00


