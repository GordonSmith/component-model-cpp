# FindWAMR.cmake
# Locates and validates WAMR runtime libraries
#
# Provides:
#   WAMR_FOUND - TRUE if WAMR was found
#   WAMR_INCLUDE_DIRS - WAMR include directories
#   WAMR_LIBRARIES - WAMR libraries to link against
#   wamr::runtime - Imported interface target

if(NOT DEFINED VCPKG_INSTALLED_DIR OR NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(WAMR_FOUND FALSE)
    return()
endif()

# Find WAMR headers
find_path(WAMR_INCLUDE_DIRS
    NAMES wasm_export.h
    PATHS ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include
    NO_DEFAULT_PATH
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

find_library(WAMR_IWASM_LIB_RELEASE NAMES ${_wamr_iwasm_names}
    PATHS ${_wamr_lib_dirs}
    NO_DEFAULT_PATH
)
find_library(WAMR_IWASM_LIB_DEBUG NAMES ${_wamr_iwasm_names}
    PATHS ${_wamr_lib_dirs_debug}
    NO_DEFAULT_PATH
)

set(WAMR_IWASM_LIB "")
if(WAMR_IWASM_LIB_RELEASE)
    set(WAMR_IWASM_LIB ${WAMR_IWASM_LIB_RELEASE})
elseif(WAMR_IWASM_LIB_DEBUG)
    set(WAMR_IWASM_LIB ${WAMR_IWASM_LIB_DEBUG})
endif()

find_library(WAMR_VMLIB_LIB_RELEASE NAMES ${_wamr_vmlib_names}
    PATHS ${_wamr_lib_dirs}
    NO_DEFAULT_PATH
)
find_library(WAMR_VMLIB_LIB_DEBUG NAMES ${_wamr_vmlib_names}
    PATHS ${_wamr_lib_dirs_debug}
    NO_DEFAULT_PATH
)

set(WAMR_VMLIB_LIB "")
if(WAMR_VMLIB_LIB_RELEASE)
    set(WAMR_VMLIB_LIB ${WAMR_VMLIB_LIB_RELEASE})
elseif(WAMR_VMLIB_LIB_DEBUG)
    set(WAMR_VMLIB_LIB ${WAMR_VMLIB_LIB_DEBUG})
endif()

# Determine if WAMR was found
if(WAMR_INCLUDE_DIRS AND WAMR_IWASM_LIB)
    set(WAMR_FOUND TRUE)
    
    # Build library list
    set(WAMR_LIBRARIES ${WAMR_IWASM_LIB})
    if(WAMR_VMLIB_LIB)
        list(APPEND WAMR_LIBRARIES ${WAMR_VMLIB_LIB})
    endif()
    if(NOT WIN32)
        list(APPEND WAMR_LIBRARIES m)
    endif()
    
    # Create imported target
    if(NOT TARGET wamr::runtime)
        add_library(wamr::runtime INTERFACE IMPORTED)
        set_target_properties(wamr::runtime PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${WAMR_INCLUDE_DIRS}"
            INTERFACE_LINK_LIBRARIES "${WAMR_LIBRARIES}"
        )
    endif()
    
    if(NOT WAMR_FIND_QUIETLY)
        message(STATUS "Found WAMR:")
        message(STATUS "  Include dirs: ${WAMR_INCLUDE_DIRS}")
        message(STATUS "  IWASM library: ${WAMR_IWASM_LIB}")
        if(WAMR_VMLIB_LIB)
            message(STATUS "  VMLIB library: ${WAMR_VMLIB_LIB}")
        endif()
    endif()
else()
    set(WAMR_FOUND FALSE)
    if(NOT WAMR_FIND_QUIETLY)
        if(WAMR_FIND_REQUIRED)
            message(FATAL_ERROR "WAMR not found. Install via: vcpkg install wasm-micro-runtime")
        else()
            message(STATUS "WAMR not found")
        endif()
    endif()
endif()

# Standard CMake find package handling
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WAMR
    REQUIRED_VARS WAMR_INCLUDE_DIRS WAMR_IWASM_LIB
    FAIL_MESSAGE "WAMR not found. Install via: vcpkg install wasm-micro-runtime"
)

mark_as_advanced(
    WAMR_INCLUDE_DIRS
    WAMR_IWASM_LIB_RELEASE
    WAMR_IWASM_LIB_DEBUG
    WAMR_VMLIB_LIB_RELEASE
    WAMR_VMLIB_LIB_DEBUG
)
