project(wasmtime C CXX ASM)

# Find Wasmtime libraries
find_path(WASMTIME_C_INCLUDE_DIRS "wasmtime.h"
    PATHS ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include/wasmtime-c-api
)
find_library(WASMTIME_LIB NAMES wasmtime
    PATHS ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib
)

if(NOT WASMTIME_C_INCLUDE_DIRS)
    message(FATAL_ERROR "Wasmtime C include directory not found. Please check Wasmtime installation.")
endif()

if(NOT WASMTIME_LIB)
    message(FATAL_ERROR "Wasmtime library not found. Please check Wasmtime installation.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_LIST_DIR}/../../include
)

add_executable(wasmtime
    main.cpp
)

target_include_directories(wasmtime 
    PRIVATE ${WASMTIME_C_INCLUDE_DIRS}
)

message("WASMTIME_C_INCLUDE_DIRS: ${WASMTIME_C_INCLUDE_DIRS}")
message("WASMTIME_LIB: ${WASMTIME_LIB}")

target_link_libraries(wasmtime
    PRIVATE ${WASMTIME_LIB}
    PRIVATE ${CMAKE_DL_LIBS}
    PRIVATE pthread
    PRIVATE m
)

# Set runtime path for the wasmtime library
if(UNIX AND NOT APPLE)
    set_target_properties(wasmtime PROPERTIES
        INSTALL_RPATH_USE_LINK_PATH TRUE
        BUILD_RPATH "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib"
    )
endif()

# Add compiler options for better error reporting
target_compile_options(wasmtime PRIVATE 
    -Wno-error=maybe-uninitialized
    -Wno-error=jump-misses-init
    -Wno-unused-parameter
)
