cmake_minimum_required(VERSION 3.2)

project(server)

file(GLOB_RECURSE SOURCES "*.c" "*.cpp" "*.h" "*.hpp")

include_directories(${CMAKE_SOURCE_DIR}/src)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} netlib)
