cmake_minimum_required(VERSION 3.14...3.22)

project(component-model LANGUAGES CXX)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
    )
endif()

enable_testing()

add_subdirectory(src)
add_subdirectory(test)
