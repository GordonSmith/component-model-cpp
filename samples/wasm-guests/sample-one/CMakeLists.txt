# ============================================================================
# Sample One - WebAssembly Guest Module
# ============================================================================
# Builds a Component Model guest from a WIT interface definition.
# Uses wit-bindgen to generate C bindings, then compiles to a core module
# and converts to a component using wasm-tools.
# ============================================================================

# Set sample-specific variables
set(SAMPLE_NAME "sample")
set(WIT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/sample.wit")

# ============================================================================
# Generate Guest Bindings from WIT
# ============================================================================

set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/${SAMPLE_NAME}")
set(GENERATED_C "${GENERATED_DIR}/${SAMPLE_NAME}.c")
set(GENERATED_H "${GENERATED_DIR}/${SAMPLE_NAME}.h")
set(GENERATED_OBJ "${GENERATED_DIR}/${SAMPLE_NAME}_component_type.o")

add_custom_command(
    OUTPUT ${GENERATED_C} ${GENERATED_H} ${GENERATED_OBJ}
    COMMAND ${WIT_BINDGEN_EXECUTABLE} c 
        --out-dir "${GENERATED_DIR}" 
        "${WIT_FILE}"
    DEPENDS ${WIT_FILE}
    COMMENT "Generating guest C bindings from ${WIT_FILE}"
    VERBATIM
)

add_custom_target(generate-${SAMPLE_NAME}-guest-bindings
    DEPENDS ${GENERATED_C} ${GENERATED_H} ${GENERATED_OBJ}
)

# ============================================================================
# Build Core WebAssembly Module
# ============================================================================

add_executable(${SAMPLE_NAME}
    main.cpp
    ${GENERATED_C}
)

add_dependencies(${SAMPLE_NAME} generate-${SAMPLE_NAME}-guest-bindings)

target_include_directories(${SAMPLE_NAME} PRIVATE
    ${GENERATED_DIR}
)

# Link component type object file
target_link_libraries(${SAMPLE_NAME}
    ${GENERATED_OBJ}
)

# Configure WASI-specific linker flags
target_link_options(${SAMPLE_NAME} PRIVATE
    -Wl,--no-entry          # No main() function (component model uses exports)
    -Wl,--export-dynamic    # Export all symbols
)

# ============================================================================
# Convert Core Module to Component
# ============================================================================
# Use wasm-tools to wrap the core module in a component wrapper that
# implements the Component Model ABI.
# ============================================================================

if (WASM_TOOLS_EXECUTABLE)
    set(COMPONENT_WASM "${CMAKE_CURRENT_BINARY_DIR}/${SAMPLE_NAME}-component.wasm")
    
    add_custom_command(
        TARGET ${SAMPLE_NAME} POST_BUILD
        COMMAND ${WASM_TOOLS_EXECUTABLE} component new 
            "$<TARGET_FILE:${SAMPLE_NAME}>" 
            -o "${COMPONENT_WASM}"
        COMMENT "Converting core module to component: ${COMPONENT_WASM}"
        VERBATIM
    )
    
    # Create a custom target for the component
    add_custom_target(${SAMPLE_NAME}-component
        DEPENDS ${SAMPLE_NAME}
    )
    
    message(STATUS "Sample '${SAMPLE_NAME}' will produce component: ${COMPONENT_WASM}")
else()
    message(STATUS "Sample '${SAMPLE_NAME}' will produce core module only (wasm-tools not found)")
endif()

# ============================================================================
# Install Target (Optional)
# ============================================================================

install(TARGETS ${SAMPLE_NAME}
    RUNTIME DESTINATION bin/wasm-guests
)

if (WASM_TOOLS_EXECUTABLE AND EXISTS "${COMPONENT_WASM}")
    install(FILES "${COMPONENT_WASM}"
        DESTINATION bin/wasm-guests
    )
endif()
