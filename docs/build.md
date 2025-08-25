## Building and Running the OS

### Prerequisites
- Install the i686 GCC cross-compiler and build utilities. Follow the official guide: [OSDev Wiki: GCC Cross-Compiler](https://wiki.osdev.org/GCC_Cross-Compiler)
- Install QEMU for emulation:
	```bash
	sudo apt-get install qemu-system-x86
	```

### Build Pipeline
From the project root, you can use the following scripts:

- **Full pipeline:**
	```bash
	./run.sh
	```
	This will build, package, and run the OS in QEMU.

- **Individual steps:**
	```bash
	./scripts/build.sh      # Build kernel.elf
	./scripts/pack.sh       # Package kernel.elf into MyOS.iso
	./scripts/run_qemu.sh   # Run QEMU (add -d for debug mode)
	./scripts/clean.sh      # Remove build and iso artifacts
	```

### Debugging
To run QEMU in debug mode (GDB integration):
```bash
./run.sh -d
```
or
```bash
./scripts/run_qemu.sh -d
```

### Output
- Build artifacts are placed in the `build/` directory.
- The bootable ISO image is `build/myos.iso`.