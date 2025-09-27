include(ExternalProject)

set(WAMR_SAMPLE_AVAILABLE FALSE)

add_subdirectory(wamr)

if (WAMR_SAMPLE_AVAILABLE)
    if (NOT WASI_SDK_PREFIX)
        message(FATAL_ERROR "WASI_SDK_PREFIX is not set. Install wasi-sdk via vcpkg or set WASI_SDK_PREFIX manually before building the samples.")
    endif()

    ExternalProject_Add(wasm
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/wasm
        BUILD_ALWAYS TRUE
        PREFIX wasm
        CMAKE_ARGS 
            -DHPCC_WASM_ROOT=${HPCC_WASM_ROOT}
            -DWASI_SDK_PREFIX=${WASI_SDK_PREFIX}
            -DCMAKE_TOOLCHAIN_FILE=${WASI_SDK_PREFIX}/share/cmake/wasi-sdk.cmake
            -DCMAKE_SYSROOT=${WASI_SDK_PREFIX}/share/wasi-sysroot
            -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    )

    add_dependencies(wamr wasm)
endif()

add_custom_target(samples)
add_dependencies(samples wamr)
