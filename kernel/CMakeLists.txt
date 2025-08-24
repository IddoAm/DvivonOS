file(GLOB SOURCES "*.c")

add_library(kernel OBJECT ${SOURCES})

target_include_directories(kernel PUBLIC ${CMAKE_SOURCE_DIR}/include)
