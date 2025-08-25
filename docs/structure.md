# Project Directory Structure

This document explains the organization of the OS project directory and the purpose of each folder and key file.

```
os/
├── arch/           # Architecture-specific code (e.g., i686)
│   └── i686/
│       ├── linker.ld      # Linker script for i686
│       ├── boot/          # Assembly code for booting
│       └── cpu/           # CPU setup and interrupt handling
├── boot/           # Bootloader configuration (GRUB)
├── build/          # Build artifacts (created automatically)
├── docs/           # Documentation
├── drivers/        # Hardware drivers (e.g., keyboard, VGA)
├── include/        # Project-wide header files
├── kernel/         # Kernel source code
├── lib/            # Standard library code and headers
├── scripts/        # Build, packaging, and run scripts
├── Makefile        # (Optional) Makefile for manual builds
└── run.sh          # Main build and run pipeline script
```

## Folder Details
- **arch/**: Contains architecture-specific code, such as boot routines, CPU setup, and linker scripts. Subfolders are named after supported architectures (e.g., `i686`).
- **boot/**: Contains bootloader configuration files, typically for GRUB.
- **build/**: Stores all build outputs, including the kernel binary and ISO image. This folder is auto-generated and can be cleaned safely.
- **docs/**: Project documentation, guides, and references.
- **drivers/**: Source code for hardware drivers, such as keyboard and VGA display.
- **include/**: Header files shared across the project.
- **kernel/**: Main kernel source code.
- **lib/**: Standard library implementations and headers used by the kernel and drivers.
- **scripts/**: Utility scripts for building, packaging, running, and cleaning the project.
- **Makefile**: (If present) Manual build instructions for advanced users.
- **readme.md**: High-level project overview and quickstart instructions.
- **run.sh**: Main script to build, package, and run the OS in QEMU.

## Notes
- All build artifacts are placed in `build/`.
- The directory structure is designed for clarity, modularity, and ease of maintenance.
- You can add more architectures by creating new subfolders under `arch/`.
