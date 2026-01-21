cmake_minimum_required(VERSION 3.16)
project(MONClient)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(glfw)

add_library(glad glad/src/glad.c)
target_include_directories(glad PUBLIC glad/include)

add_executable(MONClient
    src/main.cpp
    src/SpriteRenderer.cpp
)


target_include_directories(MONClient PRIVATE glm)

target_link_libraries(MONClient glfw glad)

# Copy the assets folder next to the built executable (so relative paths work)
add_custom_command(TARGET MONClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:MONClient>/assets"
)
