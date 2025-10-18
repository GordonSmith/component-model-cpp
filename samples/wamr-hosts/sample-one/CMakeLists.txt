# ============================================================================
# Sample One - WAMR Host Application
# ============================================================================
# C++ host application that loads and executes the sample-one WebAssembly
# component using WAMR runtime.
# ============================================================================

set(SAMPLE_NAME "sample-one")
set(WIT_FILE "${CMAKE_SOURCE_DIR}/samples/wasm-guests/sample-one/sample.wit")
set(GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated")

# Extract package name from WIT file for generated file names
# The wit-codegen tool derives output names from the WIT package declaration
# For "package example:sample", files will be named "sample.*"
set(PACKAGE_BASENAME "sample")

set(GENERATED_HEADER "${GENERATED_DIR}/${PACKAGE_BASENAME}.hpp")
set(GENERATED_WAMR_HEADER "${GENERATED_DIR}/${PACKAGE_BASENAME}_wamr.hpp")
set(GENERATED_WAMR_CPP "${GENERATED_DIR}/${PACKAGE_BASENAME}_wamr.cpp")

# ============================================================================
# Generate Host Bindings from WIT
# ============================================================================

if (BUILD_WIT_CODEGEN_EXECUTABLE)
    add_custom_command(
        OUTPUT 
            ${GENERATED_HEADER} 
            ${GENERATED_WAMR_HEADER}
            ${GENERATED_WAMR_CPP}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${GENERATED_DIR}
        COMMAND ${CMAKE_COMMAND} -E chdir ${GENERATED_DIR} 
            ${BUILD_WIT_CODEGEN_EXECUTABLE} ${WIT_FILE}
        DEPENDS wit-codegen ${WIT_FILE}
        COMMENT "Generating host bindings for ${SAMPLE_NAME} from ${WIT_FILE}"
        VERBATIM
    )
    
    add_custom_target(generate-${SAMPLE_NAME}-host-bindings
        DEPENDS ${GENERATED_HEADER} ${GENERATED_WAMR_HEADER} ${GENERATED_WAMR_CPP}
    )
    
    set(BINDINGS_GENERATED TRUE)
else()
    # If wit-codegen is not available, assume pre-generated bindings exist
    if (EXISTS ${GENERATED_HEADER} AND EXISTS ${GENERATED_WAMR_CPP})
        message(STATUS "Using pre-generated bindings for ${SAMPLE_NAME}")
        add_custom_target(generate-${SAMPLE_NAME}-host-bindings)
        set(BINDINGS_GENERATED TRUE)
    else()
        message(WARNING "No wit-codegen and no pre-generated bindings found for ${SAMPLE_NAME}. Skipping.")
        return()
    endif()
endif()

# ============================================================================
# Build WAMR Host Executable
# ============================================================================

add_executable(${SAMPLE_NAME}
    main.cpp
    host_impl.cpp           # Host implementations of imported functions
    ${GENERATED_WAMR_CPP}   # Generated WAMR symbol registration
)

add_dependencies(${SAMPLE_NAME} generate-${SAMPLE_NAME}-host-bindings)

target_include_directories(${SAMPLE_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GENERATED_DIR}
)

target_link_libraries(${SAMPLE_NAME} PRIVATE
    cmcpp
    wamr-common
)

if (TARGET wasm-guests)
    include(ExternalProject)
    ExternalProject_Get_Property(wasm-guests BINARY_DIR)

    set(_guest_sample_dir "${BINARY_DIR}/sample-one")
    set(_guest_sample_wasm "${_guest_sample_dir}/sample.wasm")

    add_dependencies(${SAMPLE_NAME} wasm-guests)

    set(_host_wasm_dir "${CMAKE_CURRENT_BINARY_DIR}/bin")
    set(_host_wasm_path "${_host_wasm_dir}/sample.wasm")

    add_custom_command(
        OUTPUT ${_host_wasm_path}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${_host_wasm_dir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_guest_sample_wasm} ${_host_wasm_path}
        DEPENDS wasm-guests
        COMMENT "Copying sample.wasm to host runtime directory"
        VERBATIM
    )

    add_custom_target(${SAMPLE_NAME}-prepare-wasm
        DEPENDS ${_host_wasm_path}
    )

    add_dependencies(${SAMPLE_NAME} ${SAMPLE_NAME}-prepare-wasm)
endif()

# ============================================================================
# WAMR Sample Test
# ============================================================================
# Register the host sample with CTest so we can exercise it automatically.
if(BUILD_TESTING AND TARGET ${SAMPLE_NAME})
    add_test(
        NAME wamr-sample-test
        COMMAND $<TARGET_FILE:${SAMPLE_NAME}>
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    set_tests_properties(wamr-sample-test PROPERTIES
        LABELS "sample"
        TIMEOUT 30
    )
    
    message(STATUS "WAMR sample test registered")
endif()

# ============================================================================
# Export Sample Availability
# ============================================================================
# Only propagate TRUE to parent if guest tooling is available
if(WIT_BINDGEN_EXECUTABLE AND TARGET wasm-guests)
    set(WAMR_SAMPLE_AVAILABLE TRUE PARENT_SCOPE)
else()
    set(WAMR_SAMPLE_AVAILABLE FALSE PARENT_SCOPE)
endif()

