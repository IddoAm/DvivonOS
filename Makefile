# =========================================================================
# Makefile for building a simple 32-bit OS kernel with GRUB compatibility
#
# This file automates the compilation, linking, and ISO creation process.
# =========================================================================

# --- Build Configuration ---
# Define the compiler and assembler
CC = i686-elf-gcc
AS = i686-elf-as
LD = i686-elf-gcc

# Define compilation flags for the C and Assembly files
CFLAGS = -Ilib/includes -Isrc/include -ffreestanding -O2 -Wall -Wextra -Isrc
ASFLAGS = --32

# Define linker flags
LDFLAGS = -T src/core/linker.ld -ffreestanding -O2 -nostdlib -lgcc

# Define the build directories
BUILD_DIR = build
ISO_DIR = isodir
GRUB_DIR = $(ISO_DIR)/boot/grub

# Define the final executable and the ISO image name
KERNEL_BIN = myos.bin
ISO_IMAGE = myos.iso

# --- Source Files ---
# Find all source files in new structure
C_SOURCES := $(wildcard src/core/*.c)
S_SOURCES := $(wildcard src/boot/*.s)
LIB_C_SOURCES := $(wildcard lib/src/*.c)

# Automatically generate object file names from source files
C_OBJECTS := $(patsubst src/core/%.c,$(BUILD_DIR)/core_%.o,$(C_SOURCES))
S_OBJECTS := $(patsubst src/boot/%.s,$(BUILD_DIR)/boot_%.o,$(S_SOURCES))
LIB_C_OBJECTS := $(patsubst lib/src/%.c,$(BUILD_DIR)/lib_%.o,$(LIB_C_SOURCES))
OBJECTS := $(C_OBJECTS) $(S_OBJECTS) $(LIB_C_OBJECTS)

# =========================================================================
# --- Build Rules ---
# The default target. Running 'make' will build the kernel and the ISO.
.PHONY: all
all: $(ISO_IMAGE)

# Rule to create the final ISO image. 
$(ISO_IMAGE): $(KERNEL_BIN)
	@echo "Creating ISO..."
	@mkdir -p $(GRUB_DIR)
	@cp $(KERNEL_BIN) $(ISO_DIR)/boot/$(KERNEL_BIN)
	@cp src/core/grub.cfg $(GRUB_DIR)/grub.cfg
	@grub-mkrescue -o $(ISO_IMAGE) $(ISO_DIR)
	@echo "Build complete! ISO: $(ISO_IMAGE)"

# Rule to link the kernel binary. It depends on all object files.
$(KERNEL_BIN): $(OBJECTS)
	@echo "Linking..."
	@$(LD) $(LDFLAGS) -o $(KERNEL_BIN) $(OBJECTS)

# Rule to compile C source files.
# The '$<` variable is the source file (e.g., src/kernel.c)
# The '$@` variable is the target file (e.g., build/kernel.o)

$(BUILD_DIR)/core_%.o: src/core/%.c
	@mkdir -p $(BUILD_DIR)
	@echo "Compiling $< -> $@"
	@$(CC) $(CFLAGS) -I src/include -c $< -o $@

# Rule to assemble assembly source files.

$(BUILD_DIR)/boot_%.o: src/boot/%.s
	@mkdir -p $(BUILD_DIR)
	@echo "Assembling $< -> $@"
	@$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/lib_%.o: lib/src/%.c
	@mkdir -p $(BUILD_DIR)
	@echo "Compiling $< -> $@"
	@$(CC) $(CFLAGS) -c $< -o $@

# =========================================================================
# --- Utility Rules ---

# Phony targets are not files. They are just a name for a command.
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR) $(KERNEL_BIN) $(ISO_DIR) $(ISO_IMAGE)

# A convenience target to run both clean and all.
# This satisfies the requirement to clear the build folder before building.
.PHONY: rebuild
rebuild: clean all

