cmake_minimum_required(VERSION 3.16)
project(OS C ASM)

# toolchain
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_C_COMPILER i686-elf-gcc)
set(CMAKE_ASM_COMPILER i686-elf-as)
set(CMAKE_LINKER i686-elf-ld)
set(CMAKE_OBJCOPY i686-elf-objcopy)

# compiler flags
set(CMAKE_C_FLAGS "-ffreestanding -fno-stack-protector -fno-pic -Wall -Wextra -O2")
set(CMAKE_EXE_LINKER_FLAGS "-nostdlib -lgcc")

# include path for headers
include_directories(${CMAKE_SOURCE_DIR}/src/include)

# collect all source files recursively
file(GLOB_RECURSE SOURCES
    ${CMAKE_SOURCE_DIR}/src/*.c
    ${CMAKE_SOURCE_DIR}/src/*.s
)

# kernel ELF target
add_executable(kernel.elf ${SOURCES})

# linker script
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/src/arch/i686/linker.ld)
set_target_properties(kernel.elf PROPERTIES
    LINK_FLAGS "-T ${LINKER_SCRIPT}"
)

# make ISO
add_custom_target(iso
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/iso/boot/grub
    COMMAND ${CMAKE_COMMAND} -E copy kernel.elf ${CMAKE_SOURCE_DIR}/iso/boot/kernel.elf
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/src/boot/grub.cfg ${CMAKE_SOURCE_DIR}/iso/boot/grub/grub.cfg
    COMMAND grub-mkrescue -o ${CMAKE_BINARY_DIR}/myos.iso ${CMAKE_SOURCE_DIR}/iso
    DEPENDS kernel.elf
)

# Bootloader commands ___________________________________________________________________________________________________
# General bootloader commands
set(BOOT_LD  ${CMAKE_SOURCE_DIR}/bootloader/bootloader.ld)
set(STAGE1_LINKER_FLAGS "-n -nostdlib -Ttext 0x7C00 --oformat binary")
set(STAGE2_LINKER_FLAGS "-n -nostdlib -Ttext 0x7E00 --oformat binary")

# Stage 1 - Boot sector (512 bytes, loaded at 0x7C00)
set(STAGE1_ASM ${CMAKE_SOURCE_DIR}/bootloader/stage1.s)
set(STAGE1_O   ${CMAKE_BINARY_DIR}/bootloader/stage1.o)
set(STAGE1_BIN ${CMAKE_BINARY_DIR}/bootloader/stage1.bin)


add_custom_command(
    OUTPUT ${STAGE1_BIN}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/bootloader
    COMMAND ${CMAKE_ASM_COMPILER} -o ${STAGE1_O} ${STAGE1_ASM}
    COMMAND ${CMAKE_LINKER} -m elf_i386 -n -nostdlib -Ttext 0x7C00 --oformat binary -o ${STAGE1_BIN} ${STAGE1_O}
    DEPENDS ${STAGE1_ASM}
    VERBATIM
)

# Stage 2 - Second stage bootloader (loaded at 0x7E00)
set(STAGE2_ASM ${CMAKE_SOURCE_DIR}/bootloader/stage2.s)
set(STAGE2_O   ${CMAKE_BINARY_DIR}/bootloader/stage2.o)
set(STAGE2_BIN ${CMAKE_BINARY_DIR}/bootloader/stage2.bin)

add_custom_command(
    OUTPUT ${STAGE2_BIN}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/bootloader
    COMMAND ${CMAKE_ASM_COMPILER} -o ${STAGE2_O} ${STAGE2_ASM}
    COMMAND ${CMAKE_LINKER} -m elf_i386 -n -nostdlib -Ttext 0x7E00 --oformat binary -o ${STAGE2_BIN} ${STAGE2_O}
    DEPENDS ${STAGE2_ASM}
    VERBATIM
)

# Create bootable floppy image (1.44MB)
set(BOOT_IMG ${CMAKE_BINARY_DIR}/bootloader/boot.img)
add_custom_command(
    OUTPUT ${BOOT_IMG}
    COMMAND cat ${STAGE1_BIN} ${STAGE2_BIN} > ${BOOT_IMG}
    DEPENDS ${STAGE1_BIN} ${STAGE2_BIN}
    VERBATIM
)

add_custom_target(bootloader ALL DEPENDS ${BOOT_IMG})

# Kernel size configuration (must match KERNEL_SECTORS in stage2.s)
set(KERNEL_MAX_SECTORS 32 CACHE STRING "Maximum number of sectors for kernel")
set(KERNEL_MAX_SIZE ${KERNEL_MAX_SECTORS} CACHE STRING "Maximum kernel size in bytes")
math(EXPR KERNEL_MAX_SIZE "${KERNEL_MAX_SECTORS} * 512")

# Create flat binary kernel from ELF
set(KERNEL_BIN ${CMAKE_BINARY_DIR}/kernel.bin)
add_custom_command(
    OUTPUT ${KERNEL_BIN}
    COMMAND ${CMAKE_OBJCOPY} -O binary ${CMAKE_BINARY_DIR}/kernel.elf ${KERNEL_BIN}
    DEPENDS kernel.elf
    VERBATIM
)

# Check kernel size and pad if necessary
set(KERNEL_PADDED_BIN ${CMAKE_BINARY_DIR}/kernel_padded.bin)
add_custom_command(
    OUTPUT ${KERNEL_PADDED_BIN}
    COMMAND ${CMAKE_COMMAND} -E echo "Checking kernel size..."
    COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --cyan "Kernel size check:"
    COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --cyan "  Max sectors: ${KERNEL_MAX_SECTORS}"
    COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --cyan "  Max size: ${KERNEL_MAX_SIZE} bytes"
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/check_kernel_size.sh ${KERNEL_BIN} ${KERNEL_MAX_SIZE}
    COMMAND ${CMAKE_COMMAND} -E copy ${KERNEL_BIN} ${KERNEL_PADDED_BIN}
    COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --yellow "Padding kernel to ${KERNEL_MAX_SIZE} bytes..."
    COMMAND dd if=/dev/zero of=${KERNEL_PADDED_BIN} bs=1 count=${KERNEL_MAX_SIZE} seek=0 conv=notrunc 2>/dev/null || true
    COMMAND dd if=${KERNEL_BIN} of=${KERNEL_PADDED_BIN} bs=1 conv=notrunc 2>/dev/null || true
    COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --green "Kernel padded successfully."
    DEPENDS ${KERNEL_BIN}
    VERBATIM
)

# Create a complete bootable image with padded kernel
set(COMPLETE_IMG ${CMAKE_BINARY_DIR}/bootloader/complete.img)
add_custom_command(
    OUTPUT ${COMPLETE_IMG}
    COMMAND ${CMAKE_COMMAND} -E copy ${BOOT_IMG} ${COMPLETE_IMG}
    # Add padded kernel to sectors 3+ (after stage1 and stage2)
    COMMAND dd if=${KERNEL_PADDED_BIN} of=${COMPLETE_IMG} bs=512 seek=3 conv=notrunc || true
    DEPENDS ${BOOT_IMG} ${KERNEL_PADDED_BIN}
    VERBATIM
)

add_custom_target(complete_bootloader DEPENDS ${COMPLETE_IMG})

add_custom_target(run_bootloader
    COMMAND qemu-system-i386 -drive file=${BOOT_IMG},format=raw,if=floppy
    DEPENDS bootloader
)

add_custom_target(run_complete
    COMMAND qemu-system-i386 -drive file=${COMPLETE_IMG},format=raw,if=floppy
    DEPENDS complete_bootloader
)
