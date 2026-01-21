cmake_minimum_required(VERSION 3.16)
project(MONClient)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(glfw)

add_library(glad glad/src/glad.c)
target_include_directories(glad PUBLIC glad/include)

add_executable(MONClient src/main.cpp)

target_link_libraries(MONClient glfw glad)
