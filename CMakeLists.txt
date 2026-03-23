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
# Removed -lgcc from here — it must come AFTER object files at link time
set(CMAKE_EXE_LINKER_FLAGS "-nostdlib")

# include path for headers
include_directories(${CMAKE_SOURCE_DIR}/src/include)

# collect all source files recursively
file(GLOB_RECURSE SOURCES
    ${CMAKE_SOURCE_DIR}/src/*.c
    ${CMAKE_SOURCE_DIR}/src/*.s
)

# kernel ELF target
add_executable(kernel.elf ${SOURCES})

# Link libgcc AFTER objects so __udivdi3 and friends resolve correctly
target_link_libraries(kernel.elf gcc)

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
set(KERNEL_MAX_SECTORS 120 CACHE STRING "Maximum number of sectors for kernel")
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

# Compile all user programs ______________________________________________________________________________________________
# Create output directory for compiled programs
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/root-fs/public-bin)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/programs)

# NewLib configuration (static linking with C library)
set(NEWLIB_BASE "$ENV{HOME}/opt/cross/i686-elf" CACHE STRING "NewLib base directory")
set(NEWLIB_INCLUDE "${NEWLIB_BASE}/include")
set(NEWLIB_LIB "${NEWLIB_BASE}/lib")

# Shared user-space files
set(CRT0_ASM "${CMAKE_SOURCE_DIR}/tools/user-space/crt0.s")
set(SYSCALLS_C "${CMAKE_SOURCE_DIR}/tools/user-space/syscalls.c")
set(DEFAULT_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/tools/user-space/linker.ld")

# Compile crt0.s once for all programs
set(CRT0_OBJ "${CMAKE_BINARY_DIR}/programs/crt0.o")
add_custom_command(
    OUTPUT ${CRT0_OBJ}
    COMMAND ${CMAKE_ASM_COMPILER} -o ${CRT0_OBJ} ${CRT0_ASM}
    DEPENDS ${CRT0_ASM}
    VERBATIM
    COMMENT "Assembling shared crt0..."
)

# Compile syscalls.c once for all programs
set(SYSCALLS_OBJ "${CMAKE_BINARY_DIR}/programs/syscalls.o")
add_custom_command(
    OUTPUT ${SYSCALLS_OBJ}
    COMMAND ${CMAKE_C_COMPILER} -c ${SYSCALLS_C} -o ${SYSCALLS_OBJ} -I${NEWLIB_INCLUDE} -ffreestanding
    DEPENDS ${SYSCALLS_C}
    VERBATIM
    COMMENT "Compiling shared syscalls..."
)

# Macro to compile a program (supports both C and assembly)
macro(add_program PROGRAM_NAME PROGRAM_DIR)
    file(GLOB PROGRAM_ASM "${PROGRAM_DIR}/*.s")
    file(GLOB PROGRAM_C "${PROGRAM_DIR}/*.c")
    file(GLOB PROGRAM_LD "${PROGRAM_DIR}/*.ld")

    # Use custom linker script if provided, otherwise use default
    if(PROGRAM_LD)
        set(LINKER_SCRIPT ${PROGRAM_LD})
    else()
        set(LINKER_SCRIPT ${DEFAULT_LINKER_SCRIPT})
    endif()

    # Start with empty object list
    set(PROGRAM_OBJS "")

    # Only include crt0.o and syscalls.o for C programs
    if(PROGRAM_C)
        list(APPEND PROGRAM_OBJS "${CRT0_OBJ}")
        list(APPEND PROGRAM_OBJS "${SYSCALLS_OBJ}")
    endif()

    # Compile program-specific assembly files
    foreach(ASM_FILE ${PROGRAM_ASM})
        get_filename_component(ASM_NAME ${ASM_FILE} NAME_WE)
        set(ASM_OBJ "${CMAKE_BINARY_DIR}/programs/${PROGRAM_NAME}_${ASM_NAME}.o")

        add_custom_command(
            OUTPUT ${ASM_OBJ}
            COMMAND ${CMAKE_ASM_COMPILER} -o ${ASM_OBJ} ${ASM_FILE}
            DEPENDS ${ASM_FILE}
            VERBATIM
            COMMENT "Assembling ${ASM_NAME} for ${PROGRAM_NAME}..."
        )

        list(APPEND PROGRAM_OBJS "${ASM_OBJ}")
    endforeach()

    # Compile program-specific C files
    foreach(C_FILE ${PROGRAM_C})
        get_filename_component(C_NAME ${C_FILE} NAME_WE)
        set(C_OBJ "${CMAKE_BINARY_DIR}/programs/${PROGRAM_NAME}_${C_NAME}.o")

        add_custom_command(
            OUTPUT ${C_OBJ}
            COMMAND ${CMAKE_C_COMPILER} -c ${C_FILE} -o ${C_OBJ} -I${NEWLIB_INCLUDE} -ffreestanding
            DEPENDS ${C_FILE}
            VERBATIM
            COMMENT "Compiling ${C_NAME} for ${PROGRAM_NAME}..."
        )

        list(APPEND PROGRAM_OBJS "${C_OBJ}")
    endforeach()

    # Link to ELF with NewLib (only if we have objects to link)
    if(PROGRAM_OBJS)
        set(PROGRAM_ELF "${CMAKE_SOURCE_DIR}/root-fs/public-bin/${PROGRAM_NAME}")

        if(PROGRAM_C)
            # C programs: link with NewLib
            add_custom_command(
                OUTPUT ${PROGRAM_ELF}
                COMMAND ${CMAKE_LINKER} -m elf_i386 -T ${LINKER_SCRIPT} -o ${PROGRAM_ELF} ${PROGRAM_OBJS} -L${NEWLIB_LIB} -lc
                DEPENDS ${PROGRAM_OBJS} ${LINKER_SCRIPT}
                VERBATIM
                COMMENT "Linking ${PROGRAM_NAME} with NewLib..."
            )
        else()
            # Assembly-only programs: link without NewLib
            add_custom_command(
                OUTPUT ${PROGRAM_ELF}
                COMMAND ${CMAKE_LINKER} -m elf_i386 -T ${LINKER_SCRIPT} -o ${PROGRAM_ELF} ${PROGRAM_OBJS}
                DEPENDS ${PROGRAM_OBJS} ${LINKER_SCRIPT}
                VERBATIM
                COMMENT "Linking ${PROGRAM_NAME}..."
            )
        endif()

        # Add to global target
        list(APPEND PROGRAM_ELFS ${PROGRAM_ELF})
    endif()
endmacro()

# Find and compile all programs
file(GLOB PROGRAM_DIRS "${CMAKE_SOURCE_DIR}/programs/*/")
foreach(PROGRAM_DIR ${PROGRAM_DIRS})
    get_filename_component(PROGRAM_NAME ${PROGRAM_DIR} NAME)
    add_program(${PROGRAM_NAME} ${PROGRAM_DIR})
endforeach()

# Create target to build all programs
if(DEFINED PROGRAM_ELFS)
    add_custom_target(programs ALL DEPENDS ${PROGRAM_ELFS})
else()
    add_custom_target(programs)
    message(WARNING "No programs found in ${CMAKE_SOURCE_DIR}/programs/")
endif()