# FindWASISDK.cmake
# Locates WASI SDK for WebAssembly guest compilation
#
# Provides:
#   WASI_SDK_FOUND - TRUE if WASI SDK was found
#   WASI_SDK_PREFIX - Path to WASI SDK installation

function(find_wasi_sdk)
    if(NOT DEFINED WASI_SDK_PREFIX OR WASI_SDK_PREFIX STREQUAL "")
        set(_wasi_sdk_candidates)
        
        # Check vcpkg installation
        if(DEFINED VCPKG_TARGET_TRIPLET)
            list(APPEND _wasi_sdk_candidates 
                "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/wasi-sdk")
        endif()
        
        # Glob for any vcpkg installation
        file(GLOB _wasi_sdk_glob "${CMAKE_BINARY_DIR}/vcpkg_installed/*/wasi-sdk")
        list(APPEND _wasi_sdk_candidates ${_wasi_sdk_glob})
        
        # Clean up list
        list(FILTER _wasi_sdk_candidates EXCLUDE REGEX "^$")
        list(REMOVE_DUPLICATES _wasi_sdk_candidates)
        
        if(_wasi_sdk_candidates)
            list(GET _wasi_sdk_candidates 0 _wasi_sdk_default)
            set(WASI_SDK_PREFIX "${_wasi_sdk_default}" CACHE PATH 
                "Path to the WASI SDK installation" FORCE)
            set(WASI_SDK_FOUND TRUE PARENT_SCOPE)
        else()
            set(WASI_SDK_PREFIX "" CACHE PATH 
                "Path to the WASI SDK installation" FORCE)
            set(WASI_SDK_FOUND FALSE PARENT_SCOPE)
        endif()
    else()
        # WASI_SDK_PREFIX was already set
        if(EXISTS "${WASI_SDK_PREFIX}")
            set(WASI_SDK_FOUND TRUE PARENT_SCOPE)
        else()
            set(WASI_SDK_FOUND FALSE PARENT_SCOPE)
        endif()
    endif()
    
    # Propagate to parent scope
    set(WASI_SDK_PREFIX "${WASI_SDK_PREFIX}" PARENT_SCOPE)
    
    if(NOT WASI_SDK_FIND_QUIETLY)
        if(WASI_SDK_FOUND)
            message(STATUS "Found WASI SDK: ${WASI_SDK_PREFIX}")
        elseif(WASI_SDK_FIND_REQUIRED)
            message(FATAL_ERROR "WASI SDK not found. Install via: vcpkg install wasi-sdk")
        else()
            message(STATUS "WASI SDK not found")
        endif()
    endif()
endfunction()

# Execute the search
find_wasi_sdk()

# Standard CMake find package handling
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WASISDK
    REQUIRED_VARS WASI_SDK_PREFIX
    FAIL_MESSAGE "WASI SDK not found. Install via: vcpkg install wasi-sdk"
)

mark_as_advanced(WASI_SDK_PREFIX)
