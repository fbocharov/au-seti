cmake_minimum_required(VERSION 3.2)

project(netlib)

include_directories(${CMAKE_SOURCE_DIR}/src)

file(GLOB_RECURSE SOURCES "*.c" "*.cpp" "*.h" "*.hpp")

add_library(${PROJECT_NAME} STATIC ${SOURCES})

target_link_libraries(${PROJECT_NAME} commonlib)
