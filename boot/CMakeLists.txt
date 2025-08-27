file(GLOB SOURCES "*.c")

add_library(boot OBJECT ${SOURCES})

target_include_directories(boot PUBLIC ${CMAKE_SOURCE_DIR}/include)
