cmake_minimum_required(VERSION 3.10)

# ============================================================================
# WebAssembly Guest Modules
# ============================================================================
# This CMakeLists.txt serves two purposes:
# 1. When called from samples/CMakeLists.txt: Sets up ExternalProject
# 2. When invoked by ExternalProject: Builds actual guest modules
# ============================================================================

# Detect if we're being called from parent or as external project

if (NOT WASI_SDK_PREFIX)
    message(FATAL_ERROR "WASI_SDK_PREFIX is not set. Install wasi-sdk via vcpkg or set WASI_SDK_PREFIX manually.")
endif()

if (CMAKE_TOOLCHAIN_FILE MATCHES "wasi-sdk")
    # ========================================================================
    # Guest Module Build (via ExternalProject)
    # ========================================================================
    # This section runs when WASI SDK toolchain is active.
    # All targets here produce WebAssembly (.wasm) modules.
    # ========================================================================

    project(component-model-guests LANGUAGES C CXX)

    # Verify WASI SDK toolchain is active
    if (NOT DEFINED WASI_SDK_PREFIX OR WASI_SDK_PREFIX STREQUAL "")
        message(FATAL_ERROR "WASI_SDK_PREFIX must be set. This project requires the WASI SDK toolchain.")
    endif()

    # Configure WebAssembly-specific settings
    set(CMAKE_EXECUTABLE_SUFFIX ".wasm")
    set(CMAKE_SHARED_LIBRARY_SUFFIX ".wasm")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -nostartfiles -fno-exceptions")

    # Find required Rust tools for WIT processing
    find_program(WIT_BINDGEN_EXECUTABLE wit-bindgen 
        HINTS $ENV{HOME}/.cargo/bin $ENV{USERPROFILE}/.cargo/bin
        REQUIRED
    )
    find_program(WASM_TOOLS_EXECUTABLE wasm-tools 
        HINTS $ENV{HOME}/.cargo/bin $ENV{USERPROFILE}/.cargo/bin
    )

    if (NOT WASM_TOOLS_EXECUTABLE)
        message(WARNING "wasm-tools not found. Core modules will not be converted to components. Install with: cargo install wasm-tools")
    endif()

    message(STATUS "WebAssembly Guest Build Configuration:")
    message(STATUS "  WASI_SDK_PREFIX: ${WASI_SDK_PREFIX}")
    message(STATUS "  wit-bindgen: ${WIT_BINDGEN_EXECUTABLE}")
    message(STATUS "  wasm-tools: ${WASM_TOOLS_EXECUTABLE}")

    # ========================================================================
    # Add Individual Guest Samples
    # ========================================================================
    # Each sample subdirectory builds a WebAssembly component from a WIT file.
    # ========================================================================

    add_subdirectory(sample-one)

else()
    # ========================================================================
    # ExternalProject Setup (called from parent)
    # ========================================================================
    # This section runs in the host build to set up the isolated WASI build.
    # ========================================================================

    include(ExternalProject)

    # Find wit-bindgen (required for guest code generation)
    find_program(WIT_BINDGEN_EXECUTABLE wit-bindgen 
        HINTS $ENV{HOME}/.cargo/bin $ENV{USERPROFILE}/.cargo/bin
    )
    if (NOT WIT_BINDGEN_EXECUTABLE)
        message(WARNING "wit-bindgen not found. Guest builds will be skipped. Install with: cargo install wit-bindgen-cli")
    endif()

    # ========================================================================
    # Ninja Build Tool Discovery
    # ========================================================================
    # The WASI guest builds use Ninja as the generator for better
    # cross-platform support and faster builds.
    # ========================================================================

    if (NOT DEFINED NINJA_EXECUTABLE)
        set(_ninja_hint_dirs)
        set(_ninja_candidate_files)

        # Check vcpkg tools directory
        if (DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_HOST_TRIPLET)
            set(_vcpkg_tool_root "${VCPKG_INSTALLED_DIR}/${VCPKG_HOST_TRIPLET}/tools")
            if (EXISTS "${_vcpkg_tool_root}")
                file(GLOB _vcpkg_fetched_ninja
                    "${_vcpkg_tool_root}/ninja*/ninja.exe"
                    "${_vcpkg_tool_root}/ninja*/ninja"
                    "${_vcpkg_tool_root}/ninja*/bin/ninja.exe"
                    "${_vcpkg_tool_root}/ninja*/bin/ninja"
                )
                list(APPEND _ninja_candidate_files ${_vcpkg_fetched_ninja})
                foreach(_file IN LISTS _vcpkg_fetched_ninja)
                    get_filename_component(_dir "${_file}" DIRECTORY)
                    list(APPEND _ninja_hint_dirs "${_dir}")
                endforeach()
            endif()
        endif()

        # Check repository's vcpkg downloads
        set(_repo_vcpkg_dir "${CMAKE_SOURCE_DIR}/vcpkg")
        if (EXISTS "${_repo_vcpkg_dir}/downloads/tools")
            file(GLOB _repo_fetched_ninja
                "${_repo_vcpkg_dir}/downloads/tools/ninja*/ninja.exe"
                "${_repo_vcpkg_dir}/downloads/tools/ninja*/ninja"
                "${_repo_vcpkg_dir}/downloads/tools/ninja*/bin/ninja.exe"
                "${_repo_vcpkg_dir}/downloads/tools/ninja*/bin/ninja"
            )
            list(APPEND _ninja_candidate_files ${_repo_fetched_ninja})
            foreach(_file IN LISTS _repo_fetched_ninja)
                get_filename_component(_dir "${_file}" DIRECTORY)
                list(APPEND _ninja_hint_dirs "${_dir}")
            endforeach()
            list(APPEND _ninja_hint_dirs "${_repo_vcpkg_dir}/downloads/tools")
        endif()

        # Try to find in hint directories first
        if (_ninja_hint_dirs)
            list(REMOVE_DUPLICATES _ninja_hint_dirs)
            find_program(NINJA_EXECUTABLE ninja
                HINTS ${_ninja_hint_dirs}
                NO_DEFAULT_PATH
            )
        endif()

        # Fall back to checking candidate files directly
        if (NOT NINJA_EXECUTABLE)
            foreach(_candidate IN LISTS _ninja_candidate_files)
                if (EXISTS "${_candidate}")
                    set(NINJA_EXECUTABLE "${_candidate}")
                    break()
                endif()
            endforeach()
        endif()

        # Last resort: system PATH
        if (NOT NINJA_EXECUTABLE)
            find_program(NINJA_EXECUTABLE ninja)
        endif()

        if (NOT NINJA_EXECUTABLE)
            message(WARNING 
                "Ninja build tool not found. Guest builds will be skipped. Install via:\n"
                "  - vcpkg: ./vcpkg/vcpkg fetch ninja\n"
                "  - system: apt-get install ninja-build (Linux) or brew install ninja (macOS)\n"
                "  - or set NINJA_EXECUTABLE manually"
            )
        endif()
        
        if (NINJA_EXECUTABLE)
            message(STATUS "Found Ninja: ${NINJA_EXECUTABLE}")
        endif()
    endif()

    # ========================================================================
    # Create ExternalProject for Guest Builds
    # ========================================================================

    if (WIT_BINDGEN_EXECUTABLE AND NINJA_EXECUTABLE)
        ExternalProject_Add(wasm-guests
            SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
            BUILD_ALWAYS TRUE
            PREFIX ${CMAKE_CURRENT_BINARY_DIR}/wasm
            CMAKE_GENERATOR Ninja
            CMAKE_ARGS
                -DWASI_SDK_PREFIX=${WASI_SDK_PREFIX}
                -DCMAKE_TOOLCHAIN_FILE=${WASI_SDK_PREFIX}/share/cmake/wasi-sdk.cmake
                -DCMAKE_SYSROOT=${WASI_SDK_PREFIX}/share/wasi-sysroot
                -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}
                -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                -DCMAKE_MAKE_PROGRAM=${NINJA_EXECUTABLE}
            INSTALL_COMMAND ""  # Don't install, just build
            LOG_CONFIGURE ON
            LOG_BUILD ON
        )
        
        # Make WAMR samples depend on guest builds
        if (TARGET wamr-samples)
            add_dependencies(wamr-samples wasm-guests)
        endif()
        
        message(STATUS "WebAssembly guest builds configured via ExternalProject")
    else()
        if (NOT WIT_BINDGEN_EXECUTABLE)
            message(WARNING "Skipping WebAssembly guest builds: wit-bindgen not found")
        endif()
        if (NOT NINJA_EXECUTABLE)
            message(WARNING "Skipping WebAssembly guest builds: ninja not found")
        endif()
    endif()

endif()
