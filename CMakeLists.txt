cmake_minimum_required(VERSION 3.16)
project(OS C ASM)

# toolchain
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_C_COMPILER i686-elf-gcc)
set(CMAKE_ASM_COMPILER i686-elf-as)

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

# bootloader flat binary (boot.bin)
set(BOOT_ASM ${CMAKE_SOURCE_DIR}/bootloader/boot.s)
set(BOOT_LD  ${CMAKE_SOURCE_DIR}/bootloader/bootloader.ld)
set(BOOT_O   ${CMAKE_BINARY_DIR}/bootloader/boot.o)
set(BOOT_ELF ${CMAKE_BINARY_DIR}/bootloader/boot.elf)
set(BOOT_BIN ${CMAKE_BINARY_DIR}/bootloader/boot.bin)

add_custom_command(
    OUTPUT ${BOOT_O}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/bootloader
    COMMAND ${CMAKE_ASM_COMPILER} -o ${BOOT_O} ${BOOT_ASM}
    DEPENDS ${BOOT_ASM}
    VERBATIM
)

add_custom_command(
    OUTPUT ${BOOT_ELF}
    COMMAND i686-elf-ld -m elf_i386 -T ${BOOT_LD} -nostdlib -n -o ${BOOT_ELF} ${BOOT_O}
    DEPENDS ${BOOT_O} ${BOOT_LD}
    VERBATIM
)

add_custom_command(
    OUTPUT ${BOOT_BIN}
    COMMAND i686-elf-objcopy -O binary ${BOOT_ELF} ${BOOT_BIN}
    DEPENDS ${BOOT_ELF}
    VERBATIM
)

# Stage 1 - Boot sector (512 bytes, loaded at 0x7C00)
set(STAGE1_ASM ${CMAKE_SOURCE_DIR}/bootloader/stage1.s)
set(STAGE1_O   ${CMAKE_BINARY_DIR}/bootloader/stage1.o)
set(STAGE1_ELF ${CMAKE_BINARY_DIR}/bootloader/stage1.elf)
set(STAGE1_BIN ${CMAKE_BINARY_DIR}/bootloader/stage1.bin)

add_custom_command(
    OUTPUT ${STAGE1_O}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/bootloader
    COMMAND ${CMAKE_ASM_COMPILER} -o ${STAGE1_O} ${STAGE1_ASM}
    DEPENDS ${STAGE1_ASM}
    VERBATIM
)
add_custom_command(
    OUTPUT ${STAGE1_ELF}
    COMMAND i686-elf-ld -m elf_i386 -T ${BOOT_LD} -nostdlib -n -o ${STAGE1_ELF} ${STAGE1_O}
    DEPENDS ${STAGE1_O} ${BOOT_LD}
    VERBATIM
)
add_custom_command(
    OUTPUT ${STAGE1_BIN}
    COMMAND i686-elf-objcopy -O binary ${STAGE1_ELF} ${STAGE1_BIN}
    DEPENDS ${STAGE1_ELF}
    VERBATIM
)

# Stage 2 - Second stage bootloader (loaded at 0x7E00)
set(STAGE2_ASM ${CMAKE_SOURCE_DIR}/bootloader/stage2.s)
set(STAGE2_O   ${CMAKE_BINARY_DIR}/bootloader/stage2.o)
set(STAGE2_ELF ${CMAKE_BINARY_DIR}/bootloader/stage2.elf)
set(STAGE2_BIN ${CMAKE_BINARY_DIR}/bootloader/stage2.bin)

add_custom_command(
    OUTPUT ${STAGE2_O}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/bootloader
    COMMAND ${CMAKE_ASM_COMPILER} -o ${STAGE2_O} ${STAGE2_ASM}
    DEPENDS ${STAGE2_ASM}
    VERBATIM
)
add_custom_command(
    OUTPUT ${STAGE2_ELF}
    COMMAND i686-elf-ld -m elf_i386 -T ${BOOT_LD} -nostdlib -n -o ${STAGE2_ELF} ${STAGE2_O}
    DEPENDS ${STAGE2_O} ${BOOT_LD}
    VERBATIM
)
add_custom_command(
    OUTPUT ${STAGE2_BIN}
    COMMAND i686-elf-objcopy -O binary ${STAGE2_ELF} ${STAGE2_BIN}
    DEPENDS ${STAGE2_ELF}
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

# Create a complete bootable image with kernel and padd the kernel if needed to be 64 sectors (32768 bytes)
set(COMPLETE_IMG ${CMAKE_BINARY_DIR}/bootloader/complete.img)
add_custom_command(
    OUTPUT ${COMPLETE_IMG}
    COMMAND ${CMAKE_COMMAND} -E copy ${BOOT_IMG} ${COMPLETE_IMG}
    # Add kernel to sectors 3+ (after stage1 and stage2) and pad the kernel if needed to be 64 sectors (32768 bytes)
    COMMAND dd if=${CMAKE_BINARY_DIR}/kernel.elf of=${COMPLETE_IMG} bs=512 seek=3 conv=notrunc || true
    COMMAND dd if=/dev/zero of=${COMPLETE_IMG} bs=512 count=64 seek=32768 conv=notrunc || true
    DEPENDS ${BOOT_IMG} kernel.elf
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
