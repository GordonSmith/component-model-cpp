cmake_minimum_required(VERSION 3.10)

# ============================================================================
# Samples
# ============================================================================
# This directory contains both WebAssembly guest samples and host samples


# Find WASI SDK for guest compilation
include(FindWASISDK)
find_wasi_sdk()

# Add subdirectories
add_subdirectory(wasm-guests)
add_subdirectory(wamr-hosts)

# Top-level samples target
add_custom_target(samples)

if(TARGET wasm-guests)
    add_dependencies(samples wasm-guests)
endif()

if(TARGET wamr-hosts)
    add_dependencies(samples wamr-hosts)
endif()
