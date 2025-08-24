file(GLOB SOURCES "*.c" "*.s" "../boot/*.s")
add_library(cpu OBJECT ${SOURCES})
