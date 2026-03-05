# Building and Running the OS

## Prerequisites

- **i686 GCC cross-compiler** and build tools. See [OSDev Wiki: GCC Cross-Compiler](https://wiki.osdev.org/GCC_Cross-Compiler). Ensure the cross-compiler is on your `PATH` (e.g. `export PATH="$HOME/opt/cross/bin:$PATH"`).
- **NewLib** C library for i686. You can build and install it yourself, 
  ```bash
  wget ftp://sourceware.org/pub/newlib/newlib-4.6.0.20260123.tar.gz
  ```
  then extract the tar. and move to a new directory (e.g. `newlib-build/`):
  ```bash
  mkdir build-newlib
  cd build-newlib
  ```
  now we can build it with the following command (adjust paths as needed):
  ```bash
  ../newlib-cygwin/newlib/configure --target=i686-elf \
    --prefix=$HOME/opt/cross \
    --disable-newlib-supplied-syscalls \
    --disable-nls \
    --enable-newlib-reent-small \
    --disable-malloc-debugging \
    --disable-newlib-multithread \
    --disable-shared \
    --enable-static
    ```
  now run the install:
  ```bash
  make
  make install
  ```

  now check that everything is set up correctly:
  ```bash
  ls /home/linux/opt/cross/lib
  ```
  expect to see `libc.a  libg.a  libm.a` among the output.
- **QEMU** (i386):
  ```bash
  sudo apt-get install qemu-system-x86
  ```
- For rebuilding the ext2 disk from `root-fs/`: **e2fsprogs** (e.g. `mkfs.ext2`):
  ```bash
  sudo apt-get install e2fsprogs
  ```

## Build system

The project uses **CMake**. The root `run.sh` script drives the full pipeline; individual steps are in `scripts/`.

## Full pipeline

From the project root:

```bash
./run.sh
```

This will:

1. Create the ext2 disk from `root-fs/` if `build/ext2disk.img` is missing.
2. Build the **custom bootloader + kernel** (default), or with `--grub` build the kernel and pack a GRUB ISO.
3. Start QEMU with the boot image and the ext2 disk attached.

## run.sh options

| Option | Short | Description |
|--------|--------|-------------|
| `--grub` | — | Use GRUB ISO as bootloader (builds kernel + `build/myos.iso`). |
| `--gdb` | `-g` | Start QEMU with GDB stub (`-S -s`). Connect with e.g. `gdb -x scripts/debug_memory.gdb`. |
| `--no-close` | `-n` | Use `-no-reboot -no-shutdown` so QEMU does not exit on reboot/shutdown. |
| `--clean` | `-c` | Clean the build directory before building. |
| `--reset-fs` | `-r` | Rebuild the ext2 disk image from `root-fs/` before building. |
| `--help` | `-h` | Print usage. |

Examples:

```bash
./run.sh                    # Default: custom bootloader, run QEMU
./run.sh --grub             # Build and run from GRUB ISO
./run.sh -g                 # Run with GDB stub
./run.sh -c                 # Clean then build and run
./run.sh -r                 # Rebuild ext2 from root-fs/ then build and run
./run.sh -c -r              # Clean, rebuild ext2, then build and run
```

## Individual steps

You can run scripts in `scripts/` yourself (after `source scripts/set_env.sh` or letting `run.sh` do it):

| Step | Script | Description |
|------|--------|-------------|
| Clean | `./scripts/clean.sh` | Remove `build/` (and `iso/` if present). |
| Ext2 disk | `./scripts/create_rootfs.sh` | Create `build/ext2disk.img` from `root-fs/`. |
| Build (default) | `./scripts/build_bootloader.sh` | Configure CMake and build bootloader + kernel → `build/bootloader/complete.img`. |
| Build (GRUB) | `./scripts/build_grub.sh` then `./scripts/pack.sh` | Build kernel and pack `build/myos.iso`. |
| Run QEMU | `./scripts/run_qemu.sh` *args* | Start QEMU; pass options (e.g. `-S -s` for GDB) as *args*. |

There is no standalone `build.sh`; use `build_bootloader.sh` or `build_grub.sh` as above.

## Debugging

Run with GDB stub:

```bash
./run.sh -g
```

Then attach from another terminal, e.g.:

```bash
gdb -x scripts/debug_memory.gdb
```

## Output locations

| Artifact | Path |
|----------|------|
| Build directory | `build/` |
| Custom boot image | `build/bootloader/complete.img` |
| GRUB ISO | `build/myos.iso` |
| Ext2 disk image | `build/ext2disk.img` |
