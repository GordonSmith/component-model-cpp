cmake_minimum_required(VERSION 3.10)

# ============================================================================
# WAMR Host Applications
# ============================================================================
# This directory contains C++ host applications that use WAMR to execute
# WebAssembly Component Model guests.
# ============================================================================

project(wamr-samples LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================================
# Find WAMR Runtime
# ============================================================================
include(FindWAMR)
find_package(WAMR QUIET)

if(NOT WAMR_FOUND)
    message(WARNING "WAMR runtime not found. Skipping WAMR samples.")
    message(WARNING "  Install via: vcpkg install wasm-micro-runtime")
    return()
endif()

# ============================================================================
# Check for wit-codegen (for host binding generation)
# ============================================================================
find_program(BUILD_WIT_CODEGEN_EXECUTABLE wit-codegen
    HINTS ${CMAKE_BINARY_DIR}/tools/wit-codegen
    PATHS ${CMAKE_BINARY_DIR}
    DOC "wit-codegen tool for generating host bindings"
)

if(NOT BUILD_WIT_CODEGEN_EXECUTABLE)
    message(WARNING 
        "wit-codegen not found. Host bindings must be pre-generated.\n"
        "Build wit-codegen first: cmake --build build --target wit-codegen"
    )
endif()

# ============================================================================
# Create WAMR Helper Interface Library
# ============================================================================
# Provides common WAMR configuration for all samples
# ============================================================================
include(CompilerOptions)

add_library(wamr-common INTERFACE)
target_link_libraries(wamr-common INTERFACE wamr::runtime)
add_wamr_compile_options(wamr-common)

# ============================================================================
# Add Individual WAMR Sample Hosts
# ============================================================================
add_subdirectory(sample-one)

# ============================================================================
# Top-level WAMR Samples Target
# ============================================================================
add_custom_target(wamr-samples)
add_dependencies(wamr-samples sample-one)

