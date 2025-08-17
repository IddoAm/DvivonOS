# Simple OS - Quick Start

To build and run this OS, follow these steps:

1. Add the cross-compiler to your PATH:
   ```bash
   export PATH="$HOME/opt/cross/bin:$PATH"
   ```
2. Build the OS:
   ```bash
   make
   ```
3. Run the OS in QEMU:
   ```bash
   qemu-system-i386 -cdrom myos.iso
   ```
