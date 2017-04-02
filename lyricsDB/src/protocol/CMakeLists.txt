cmake_minimum_required(VERSION 3.2)

project(protolib)

file(GLOB_RECURSE SOURCES "*.c" "*.cpp" "*.h" "*.hpp")

add_library(${PROJECT_NAME} STATIC ${SOURCES})
