cmake_minimum_required(VERSION 3.16) 

project(OS C ASM) # the supported languages are C and ASM

# the tools to build the OS
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_C_COMPILER i686-elf-gcc)
set(CMAKE_ASM_COMPILER i686-elf-as)

# add the flags for the tools
set(CMAKE_C_FLAGS "-ffreestanding -fno-stack-protector -fno-pic -Wall -Wextra -O2")
set(CMAKE_EXE_LINKER_FLAGS "-nostdlib -lgcc")

# tell the compiler where to search for includes
include_directories(${CMAKE_SOURCE_DIR}/include)

# add the subdirectories - directories with their own CMakeLists.txt
add_subdirectory(lib)
add_subdirectory(drivers)
add_subdirectory(arch/i686/cpu)
add_subdirectory(kernel)

# compile the kernel to an .elf
add_executable(kernel.elf "")

# link the objects files to the elf
target_link_libraries(kernel.elf
    lib
    drivers
    cpu
    kernel
)

# set our mem map
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/arch/i686/linker.ld)
set_target_properties(kernel.elf PROPERTIES
    LINK_FLAGS "-T ${LINKER_SCRIPT}"
)

# make the actual iso image
add_custom_target(iso
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/iso/boot/grub
    COMMAND ${CMAKE_COMMAND} -E copy kernel.elf ${CMAKE_SOURCE_DIR}/iso/boot/kernel.elf
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/boot/grub.cfg ${CMAKE_SOURCE_DIR}/iso/boot/grub/grub.cfg
    COMMAND grub-mkrescue -o ${CMAKE_BINARY_DIR}/myos.iso ${CMAKE_SOURCE_DIR}/iso
    DEPENDS kernel.elf
)

