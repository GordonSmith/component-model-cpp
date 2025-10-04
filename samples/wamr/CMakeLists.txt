cmake_minimum_required(VERSION 3.10)

project(wamr LANGUAGES C CXX ASM)

include(SelectLibraryConfigurations)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(WAMR_SAMPLE_AVAILABLE FALSE)

if (NOT DEFINED VCPKG_INSTALLED_DIR OR NOT DEFINED VCPKG_TARGET_TRIPLET)
    message(FATAL_ERROR "The WAMR sample requires vcpkg to locate wasm-micro-runtime. Please configure CMake with the vcpkg toolchain file.")
endif()

find_path(WAMR_INCLUDE_DIRS
    NAMES wasm_export.h
    PATHS ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include
)

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

set(_wamr_require_vmlib FALSE)
if (WIN32)
    set(_wamr_require_vmlib FALSE)
endif()

find_library(IWASM_LIB_RELEASE NAMES ${_wamr_iwasm_names} PATHS ${_wamr_lib_dirs})
find_library(IWASM_LIB_DEBUG NAMES ${_wamr_iwasm_names} PATHS ${_wamr_lib_dirs_debug})
select_library_configurations(IWASM_LIB)
if (NOT IWASM_LIB)
    if (IWASM_LIB_RELEASE AND NOT IWASM_LIB_RELEASE MATCHES "NOTFOUND")
        set(IWASM_LIB ${IWASM_LIB_RELEASE})
    elseif (IWASM_LIB_DEBUG AND NOT IWASM_LIB_DEBUG MATCHES "NOTFOUND")
        set(IWASM_LIB ${IWASM_LIB_DEBUG})
    endif()
endif()

find_library(VMLIB_LIB_RELEASE NAMES ${_wamr_vmlib_names} PATHS ${_wamr_lib_dirs})
find_library(VMLIB_LIB_DEBUG NAMES ${_wamr_vmlib_names} PATHS ${_wamr_lib_dirs_debug})
select_library_configurations(VMLIB_LIB)
if (NOT VMLIB_LIB)
    if (VMLIB_LIB_RELEASE AND NOT VMLIB_LIB_RELEASE MATCHES "NOTFOUND")
        set(VMLIB_LIB ${VMLIB_LIB_RELEASE})
    elseif (VMLIB_LIB_DEBUG AND NOT VMLIB_LIB_DEBUG MATCHES "NOTFOUND")
        set(VMLIB_LIB ${VMLIB_LIB_DEBUG})
    endif()
endif()

set(_wamr_has_vmlib TRUE)
if (NOT VMLIB_LIB OR VMLIB_LIB MATCHES "NOTFOUND")
    set(_wamr_has_vmlib FALSE)
endif()

message (STATUS "WAMR_INCLUDE_DIRS = ${WAMR_INCLUDE_DIRS}")
message (STATUS "IWASM_LIB = ${IWASM_LIB}")
message (STATUS "VMLIB_LIB = ${VMLIB_LIB}")

if (NOT WAMR_INCLUDE_DIRS OR NOT IWASM_LIB OR (_wamr_require_vmlib AND NOT _wamr_has_vmlib))
    if (WIN32)
        message(WARNING "WAMR runtime not available in the current toolchain; the sample will be skipped.")
        add_custom_target(wamr
            COMMAND ${CMAKE_COMMAND} -E echo "Skipping WAMR host sample because wasm-micro-runtime is not installed for this platform."
        )
        set(WAMR_SAMPLE_AVAILABLE FALSE PARENT_SCOPE)
        return()
    else()
        message(FATAL_ERROR
            "Failed to locate WebAssembly Micro Runtime components."
            "\n  WAMR_INCLUDE_DIRS = ${WAMR_INCLUDE_DIRS}"
            "\n  IWASM_LIB = ${IWASM_LIB}"
            "\n  VMLIB_LIB = ${VMLIB_LIB}"
            "\nInstall the wasm-micro-runtime port via vcpkg or set these paths manually.")
    endif()
endif()

# Check if wit-bindgen is available (required for wasm guest build)
find_program(WIT_BINDGEN_EXECUTABLE wit-bindgen HINTS $ENV{HOME}/.cargo/bin)
if (NOT WIT_BINDGEN_EXECUTABLE)
    message(WARNING "wit-bindgen not found. Wasm guest samples will not be built. Install with: cargo install wit-bindgen-cli")
    set(WAMR_SAMPLE_AVAILABLE FALSE)
else()
    set(WAMR_SAMPLE_AVAILABLE TRUE)
endif()

# Generate host bindings from WIT file using wit-codegen
set(WIT_FILE "${CMAKE_SOURCE_DIR}/samples/wasm/sample.wit")
set(GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated")
# Package name 'example:sample' will be extracted to 'sample' by wit-codegen
set(GENERATED_HEADER "${GENERATED_DIR}/sample.hpp")
set(GENERATED_WAMR_BINDINGS "${GENERATED_DIR}/sample_wamr.cpp")

# Create custom command to generate bindings
# Note: wit-codegen automatically derives the output name from the package name in the WIT file
add_custom_command(
    OUTPUT ${GENERATED_HEADER} ${GENERATED_WAMR_BINDINGS}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GENERATED_DIR}
    COMMAND cd ${GENERATED_DIR} && $<TARGET_FILE:wit-codegen> ${WIT_FILE}
    DEPENDS wit-codegen ${WIT_FILE}
    COMMENT "Generating host bindings from WIT file (output name derived from package)"
    VERBATIM
)

# Create a custom target for the generated files
add_custom_target(generate-sample-host-bindings
    DEPENDS ${GENERATED_HEADER} ${GENERATED_WAMR_BINDINGS}
)

# WAMR sample executable that runs both hand-coded and generated implementations
add_executable(wamr
    main.cpp
    host_impl.cpp  # Host-provided implementations of generated interfaces
    ${GENERATED_WAMR_BINDINGS}  # Generated WAMR symbol arrays
)

# Make sure bindings are generated before building
add_dependencies(wamr generate-sample-host-bindings)

target_include_directories(wamr
    PRIVATE
        ${WAMR_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${GENERATED_DIR}  # Add generated directory to include path
)

if (MSVC)
    target_compile_options(wamr PRIVATE /EHsc /permissive-)
else()
    target_compile_options(wamr PRIVATE -Wno-error=maybe-uninitialized -Wno-error=jump-misses-init)
endif()

set(_wamr_link_libs ${IWASM_LIB})
if (_wamr_has_vmlib)
    list(APPEND _wamr_link_libs ${VMLIB_LIB})
elseif (NOT _wamr_require_vmlib)
    message(STATUS "Skipping optional VMLIB; wasm-micro-runtime on Windows does not provide it.")
endif()
if (NOT WIN32)
    list(APPEND _wamr_link_libs m)
endif()

target_link_libraries(wamr PRIVATE cmcpp ${_wamr_link_libs})

# Only propagate TRUE to parent if wit-bindgen is available
if (WIT_BINDGEN_EXECUTABLE)
    set(WAMR_SAMPLE_AVAILABLE TRUE PARENT_SCOPE)
else()
    set(WAMR_SAMPLE_AVAILABLE FALSE PARENT_SCOPE)
endif()
