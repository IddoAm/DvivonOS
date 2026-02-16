# Project Scripts Overview

Scripts live in **scripts/** except for the main entry point **run.sh** in the project root.

## run.sh (project root)

**Usage:** `./run.sh [--grub] [--gdb|-g] [--no-close|-n] [--clean|-c] [--reset-fs|-r] [-h|--help]`

Runs the full pipeline: optional clean, optional ext2 rebuild, build (bootloader or GRUB), then QEMU.

- **--grub** — Use GRUB ISO (build_grub.sh + pack.sh) instead of custom bootloader.
- **-g, --gdb** — Start QEMU with GDB stub (-S -s).
- **-n, --no-close** — QEMU: -no-reboot -no-shutdown.
- **-c, --clean** — Run clean.sh before building.
- **-r, --reset-fs** — Rebuild ext2 disk from root-fs/ before building.
- **-h, --help** — Print usage.

Requires: `build_grub.sh`, `pack.sh`, `run_qemu.sh`, `build_bootloader.sh`, `create_rootfs.sh`, `clean.sh` (all in `scripts/`).

---

## scripts/ — Build and run

| Script | Purpose |
|--------|--------|
| **set_env.sh** | Sets up environment (e.g. cross-compiler PATH). Sourced by run.sh and other scripts. |
| **build_bootloader.sh** | Configures CMake (if needed) and builds the complete custom bootloader + kernel image. Output: `build/bootloader/complete.img`. |
| **build_grub.sh** | Builds the kernel for GRUB (CMake target used for ISO build). |
| **pack.sh** | Builds the GRUB ISO target; produces `build/myos.iso`. |
| **create_rootfs.sh** | Creates `build/ext2disk.img` from the contents of `root-fs/` (uses mkfs.ext2 -d). |
| **run_qemu.sh** | Launches `qemu-system-i386` with serial to stdio, 512 MiB RAM, and any extra arguments (e.g. -cdrom, -drive, -S -s). |
| **clean.sh** | Removes `build/` and `iso/` directories. |

---

## scripts/ — Code quality and formatting

| Script / path | Purpose |
|----------------|--------|
| **formats/lint.sh** | Runs formatting and quality checks (e.g. clang-format, clang-tidy, naming). |
| **formats/.clang-format** | clang-format configuration (see [coding-standards.md](coding-standards.md)). |
| **formats/.clang-tidy** | clang-tidy configuration. |
| **check_naming.py** | Naming-convention checks for C sources. |
| **check_kernel_size.sh** | Checks kernel binary size (if used by build). |

---

## scripts/ — Debug and deploy

| Script | Purpose |
|--------|--------|
| **debug_memory.gdb** | GDB script for connecting to QEMU (e.g. when using `./run.sh -g`). |
| **deploy/sync-with-gitlab.sh** | Deployment/sync with GitLab (if used). |

---

## Notes

- Scripts are written for **bash** and use `set -euo pipefail` where appropriate.
- Build output goes to **build/**; the bootable ISO is **build/myos.iso** when using GRUB.
- For formatting and lint commands, see [coding-standards.md](coding-standards.md).
