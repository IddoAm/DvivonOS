# Grab all .c files in drivers/
file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

add_library(drivers OBJECT ${SOURCES})
target_include_directories(drivers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
