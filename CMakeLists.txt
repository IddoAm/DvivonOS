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

add_custom_target(bootloader ALL DEPENDS ${BOOT_BIN})

add_custom_target(run_bootloader
    COMMAND qemu-system-i386 -drive file=${BOOT_BIN},format=raw,if=floppy
    DEPENDS bootloader
)
