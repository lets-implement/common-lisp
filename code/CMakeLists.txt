 
cmake_minimum_required(VERSION 2.6)
project(picl)

file(GLOB SOURCES "core/*.c++")
 
add_definitions(-std=c++11)
include_directories(core/include)

add_executable(picl ${SOURCES})
