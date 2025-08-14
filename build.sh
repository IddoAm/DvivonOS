#!/bin/bash
set -e

# Create build directories if they don't exist
mkdir -p build
mkdir -p isodir/boot/grub

# Assemble bootloader
echo "Assembling boot.s..."
i686-elf-as src/boot.s -o build/boot.o

# Compile C kernel files
echo "Compiling C files..."
for file in src/*.c; do
    obj="build/$(basename ${file%.c}.o)"
    i686-elf-gcc -ffreestanding -O2 -Wall -Wextra -c $file -o $obj
done

# Link kernel + bootloader
echo "Linking..."
i686-elf-gcc -T src/linker.ld -o myos.bin -ffreestanding -O2 -nostdlib build/*.o -lgcc

# Prepare ISO
echo "Creating ISO..."
cp myos.bin isodir/boot/myos.bin
cp grub.cfg isodir/boot/grub/grub.cfg
grub-mkrescue -o myos.iso isodir

echo "Build complete! ISO: myos.iso"

