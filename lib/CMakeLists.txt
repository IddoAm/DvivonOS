# Automatically grab all .c files in lib/
file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

add_library(lib OBJECT ${SOURCES})

# Include path for headers
target_include_directories(lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
