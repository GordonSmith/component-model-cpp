project(wamr C CXX ASM)

find_path(WAMR_INCLUDE_DIRS "wasm_export.h"
    PATHS ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}
)
if (WIN32)
    find_library(WAMR_LIB NAMES iwasm.dll vmlib.dll
        PATHS ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}
    )
else()
    find_library(IWASM_LIB NAMES iwasm 
        PATHS ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}
    )
    find_library(VMLIB_LIB NAMES vmlib
        PATHS ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}
    )
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_LIST_DIR}/../../include
)

add_executable(wamr
    main.cpp
)
target_compile_options(wamr PRIVATE -Wno-error=maybe-uninitialized)
target_compile_options(wamr PRIVATE -Wno-error=jump-misses-init)

target_include_directories(wamr 
    PRIVATE ${WAMR_INCLUDE_DIRS}
)
message("VMLIB_LIB: ${VMLIB_LIB}")
message("IWASM_LIB: ${IWASM_LIB}")
target_link_libraries(wamr
    # PRIVATE ${IWASM_LIB}
    PRIVATE ${VMLIB_LIB}
    PRIVATE m
)
