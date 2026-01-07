# GDB script to debug bootloader memory
target remote 127.0.0.1:1234
set architecture i386

# Set breakpoints
# break *0x20000d
break *0xc020101c
break *0xc0203160
break *0xc0203116

# debug data
# ret of print of stage 1  *0x7c53
# ret of print of stage 2 *0x7f71
# jamp to stage tow 0x7c4c
