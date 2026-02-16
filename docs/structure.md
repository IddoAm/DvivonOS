# Project Directory Structure

This document describes the layout of the OS project and the role of each top-level directory and key area.

```
os/
├── bootloader/      # Custom bootloader source (used when not using GRUB)
├── build/           # Build output (CMake, kernel.elf, ISO, ext2 disk) — generated
├── docs/            # Project documentation
├── isodir/          # GRUB ISO layout (boot/grub, kernel) — used when building with --grub
├── root-fs/         # Contents copied into the ext2 disk image (build/ext2disk.img)
├── scripts/         # Build, pack, run, clean, and code-format scripts
├── src/             # All kernel and supporting source code
│   ├── arch/        # Architecture-specific code
│   │   └── i686/    # i686 boot, CPU (GDT, IDT), memory setup
│   ├── boot/        # Multiboot / early boot (e.g. loader)
│   ├── drivers/     # Hardware drivers (VGA, keyboard, disk/ATA, block device)
│   ├── fs/          # Filesystem layer
│   │   ├── ext2/    # ext2 implementation (superblock, inode, block, directory, ops)
│   │   └── vfs/     # Virtual filesystem (dentry, inode, superblock, mount, file, filesystem)
│   ├── include/     # Project-wide headers (mirrors src layout: arch, boot, drivers, fs, kernel, lib, prog)
│   ├── kernel/      # Core kernel (main, memory, scheduler, syscall, time)
│   ├── lib/         # In-kernel library (stdio, string, fs high-level API)
│   └── prog/        # User-space programs (shell, terminal)
├── readme.md        # Quick start
└── run.sh           # Main entry: build, pack (optional), run QEMU
```

## Top-level directories

| Directory     | Purpose |
|---------------|--------|
| **bootloader/** | Custom bootloader used when running without `--grub`. Produces a bootable floppy-style image. |
| **build/**      | All build artifacts: CMake cache, kernel.elf, bootloader image, myos.iso (when using GRUB), ext2disk.img. Safe to remove; recreated on next build. |
| **docs/**       | Documentation (structure, build, scripts, coding standards). |
| **isodir/**     | Staging for GRUB ISO: kernel.elf and boot/grub config. Used by the pack step when building with `--grub`. |
| **root-fs/**    | Directory tree copied into the ext2 disk image. Use `./run.sh --reset-fs` to rebuild ext2disk.img from this. |
| **scripts/**    | Build and tooling: set_env.sh, build_bootloader.sh, build_grub.sh, pack.sh, create_rootfs.sh, run_qemu.sh, clean.sh; formats/ (lint, clang-format, clang-tidy). |
| **src/**        | All C/asm source and headers. See below. |

## Source tree (src/)

| Path              | Purpose |
|-------------------|--------|
| **src/arch/i686/**| i686 boot, GDT/IDT, memory mapping. |
| **src/boot/**     | Early boot (e.g. multiboot loader). |
| **src/drivers/**  | VGA, keyboard, ATA disk, block device abstraction. |
| **src/fs/ext2/**  | ext2 superblock, inodes, blocks, directories, high-level ops. |
| **src/fs/vfs/**   | VFS: dentry, inode, superblock, mount, file ops, filesystem registration. |
| **src/include/**  | Public headers; layout mirrors src (arch, boot, drivers, fs, kernel, lib, prog). |
| **src/kernel/**   | Main kernel entry, memory (PMM, VMM, heap), scheduler, syscalls, time. |
| **src/lib/**      | Kernel lib: stdio, string, and high-level fs API (path-based create/read/write/list/stat). |
| **src/prog/**     | User programs: shell and terminal. |

## Notes

- The build system is **CMake**; `run.sh` invokes scripts that call `cmake` and `cmake --build` with the appropriate targets.
- Default boot path: custom bootloader → kernel.elf from a raw disk image. With `--grub`, the flow is GRUB → kernel.elf from an ISO.
- The ext2 disk image is built from `root-fs/` and attached to QEMU as a second drive for the kernel to mount and use.
