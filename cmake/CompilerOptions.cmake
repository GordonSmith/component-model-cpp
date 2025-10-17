# CompilerOptions.cmake
# Centralized compiler flags and options for the project

# Function to add coverage support to a target
function(add_coverage_support target_name)
    if(CMAKE_COMPILER_IS_GNUCXX)
        target_compile_options(${target_name} PRIVATE --coverage)
        target_link_options(${target_name} PRIVATE --coverage)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${target_name} PRIVATE -fprofile-instr-generate -fcoverage-mapping)
        target_link_options(${target_name} PRIVATE -fprofile-instr-generate)
    endif()
endfunction()

# Function to suppress WebAssembly narrowing warnings
function(suppress_wasm_narrowing_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE
            /wd4244  # conversion from 'type1' to 'type2', possible loss of data
            /wd4267  # conversion from 'size_t' to 'type', possible loss of data
            /wd4305  # truncation from 'type1' to 'type2'
            /wd4309  # truncation of constant value
        )
    endif()
endfunction()

# Function to set WAMR-specific compile options
function(add_wamr_compile_options target_name)
    get_target_property(_target_type ${target_name} TYPE)
    if(_target_type STREQUAL "INTERFACE_LIBRARY")
        set(_scope INTERFACE)
    else()
        set(_scope PRIVATE)
    endif()
    
    if(MSVC)
        target_compile_options(${target_name} ${_scope}
            /EHsc 
            /permissive-
            /wd4244  # conversion warnings
            /wd4267  # size_t conversion warnings
            /wd4305  # truncation warnings
            /wd4309  # constant truncation warnings
        )
    else()
        target_compile_options(${target_name} ${_scope}
            -Wno-error=maybe-uninitialized
            -Wno-error=jump-misses-init
        )
    endif()
endfunction()
