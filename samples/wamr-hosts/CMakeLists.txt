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

include(SelectLibraryConfigurations)

# ============================================================================
# Find WAMR Runtime (via vcpkg)
# ============================================================================

if (NOT DEFINED VCPKG_INSTALLED_DIR OR NOT DEFINED VCPKG_TARGET_TRIPLET)
    message(WARNING "vcpkg not detected. WAMR samples require vcpkg for dependency management.")
    return()
endif()

# Find WAMR headers
find_path(WAMR_INCLUDE_DIRS
    NAMES wasm_export.h
    PATHS ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include
)

# Find WAMR libraries
set(_wamr_lib_dirs
    ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib
    ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib/manual-link
    ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin
)
set(_wamr_lib_dirs_debug
    ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/lib
    ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/lib/manual-link
    ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin
)

set(_wamr_iwasm_names iwasm libiwasm iwasm_static)
set(_wamr_vmlib_names vmlib libvmlib vmlib_static)

find_library(IWASM_LIB_RELEASE NAMES ${_wamr_iwasm_names}
    PATHS ${_wamr_lib_dirs}
    NO_DEFAULT_PATH
)
find_library(IWASM_LIB_DEBUG NAMES ${_wamr_iwasm_names}
    PATHS ${_wamr_lib_dirs_debug}
    NO_DEFAULT_PATH
)

set(IWASM_LIB "")
if (IWASM_LIB_RELEASE)
    set(IWASM_LIB ${IWASM_LIB_RELEASE})
elseif (IWASM_LIB_DEBUG)
    set(IWASM_LIB ${IWASM_LIB_DEBUG})
endif()

find_library(VMLIB_LIB_RELEASE NAMES ${_wamr_vmlib_names}
    PATHS ${_wamr_lib_dirs}
    NO_DEFAULT_PATH
)
find_library(VMLIB_LIB_DEBUG NAMES ${_wamr_vmlib_names}
    PATHS ${_wamr_lib_dirs_debug}
    NO_DEFAULT_PATH
)

set(VMLIB_LIB "")
if (VMLIB_LIB_RELEASE)
    set(VMLIB_LIB ${VMLIB_LIB_RELEASE})
elseif (VMLIB_LIB_DEBUG)
    set(VMLIB_LIB ${VMLIB_LIB_DEBUG})
endif()

# Platform-specific library requirements
set(_wamr_require_vmlib FALSE)
if (WIN32)
    set(_wamr_require_vmlib FALSE)  # Windows typically uses iwasm only
endif()

# Validate WAMR availability
if (NOT WAMR_INCLUDE_DIRS OR NOT IWASM_LIB)
    message(WARNING 
        "WAMR runtime not found. Skipping WAMR samples.\n"
        "  WAMR_INCLUDE_DIRS: ${WAMR_INCLUDE_DIRS}\n"
        "  IWASM_LIB: ${IWASM_LIB}\n"
        "  VMLIB_LIB: ${VMLIB_LIB}\n"
        "Install wasm-micro-runtime via vcpkg: vcpkg install wasm-micro-runtime"
    )
    return()
endif()

if (_wamr_require_vmlib AND (NOT VMLIB_LIB OR VMLIB_LIB MATCHES "NOTFOUND"))
    message(WARNING "WAMR vmlib not found (required on this platform). Skipping WAMR samples.")
    return()
endif()

message(STATUS "WAMR Host Build Configuration:")
message(STATUS "  WAMR_INCLUDE_DIRS: ${WAMR_INCLUDE_DIRS}")
message(STATUS "  IWASM_LIB: ${IWASM_LIB}")
message(STATUS "  VMLIB_LIB: ${VMLIB_LIB}")

# ============================================================================
# Check for wit-codegen (for host binding generation)
# ============================================================================

find_program(WIT_CODEGEN_EXECUTABLE wit-codegen
    HINTS ${CMAKE_BINARY_DIR}/tools/wit-codegen
    PATHS ${CMAKE_BINARY_DIR}
    DOC "wit-codegen tool for generating host bindings"
)

if (NOT WIT_CODEGEN_EXECUTABLE)
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

add_library(wamr-common INTERFACE)

target_include_directories(wamr-common INTERFACE
    ${WAMR_INCLUDE_DIRS}
)

target_link_libraries(wamr-common INTERFACE
    ${IWASM_LIB}
    $<$<BOOL:${VMLIB_LIB}>:${VMLIB_LIB}>
)

# Platform-specific compile options
if (MSVC)
    target_compile_options(wamr-common INTERFACE
        /EHsc 
        /permissive-
        /wd4244  # conversion warnings
        /wd4267  # size_t conversion warnings
        /wd4305  # truncation warnings
        /wd4309  # constant truncation warnings
    )
else()
    target_compile_options(wamr-common INTERFACE
        -Wno-error=maybe-uninitialized
        -Wno-error=jump-misses-init
    )
endif()

# ============================================================================
# Add Individual WAMR Sample Hosts
# ============================================================================

add_subdirectory(sample-one)

# ============================================================================
# Top-level WAMR Samples Target
# ============================================================================

add_custom_target(wamr-samples)
add_dependencies(wamr-samples sample-one)
