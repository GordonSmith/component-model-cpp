# ===== WIT Stub Generation and Validation =====
# This module generates C++ stubs from WIT test files and compiles them to validate
# the code generation pipeline (grammar -> parser -> codegen -> compilation)
#
# Requirements:
# - BUILD_GRAMMAR=ON (for wit-codegen tool)
# - wit-codegen target must exist
# - Python 3 (for stub generation script)
#
# Provides:
# - generate-test-stubs: Generate stubs from all WIT test files
# - validate-test-stubs: Compile generated stubs to verify code generation
# - test-stubs-full: Combined generation + validation

# Only proceed if grammar/codegen tools are available
if(NOT BUILD_GRAMMAR OR NOT TARGET wit-codegen)
    message(STATUS "wit-codegen not available, skipping stub generation tests")
    message(STATUS "  Enable with: cmake -DBUILD_GRAMMAR=ON")
    return()
endif()

# Check for Python 3
find_package(Python3 COMPONENTS Interpreter)
if(NOT Python3_FOUND)
    message(WARNING "Python 3 not found. Stub generation tests will not be available.")
    return()
endif()

message(STATUS "Configuring WIT stub generation tests...")

# ===== Configuration =====
set(STUB_GEN_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/generate_test_stubs.py")
set(STUB_OUTPUT_DIR "${CMAKE_BINARY_DIR}/test/generated_stubs")
set(WIT_TEST_DIR "${CMAKE_SOURCE_DIR}/ref/wit-bindgen/tests/codegen")

# Check if test directory exists
if(NOT EXISTS "${WIT_TEST_DIR}")
    message(WARNING "WIT test directory not found: ${WIT_TEST_DIR}")
    message(WARNING "  Initialize with: git submodule update --init --recursive")
    return()
endif()

# Check if generation script exists
if(NOT EXISTS "${STUB_GEN_SCRIPT}")
    message(WARNING "Stub generation script not found: ${STUB_GEN_SCRIPT}")
    return()
endif()

# ===== Target: generate-test-stubs =====
# Generates C++ stubs from all WIT test files
add_custom_target(generate-test-stubs
    COMMAND ${CMAKE_COMMAND} -E echo "Generating stubs from WIT test files..."
    COMMAND ${Python3_EXECUTABLE} "${STUB_GEN_SCRIPT}"
        --test-dir "${WIT_TEST_DIR}"
        --output-dir "${STUB_OUTPUT_DIR}"
        --codegen "$<TARGET_FILE:wit-codegen>"
        --verbose
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS wit-codegen
    COMMENT "Generating C++ stubs from WIT test suite"
    VERBATIM
)

# ===== Library: test-stubs-compiled =====
# Compiles generated stubs to validate code generation
# IMPORTANT: Compilation failures indicate bugs in wit-codegen that need fixing!
# This test intentionally tries to compile all generated code to find issues.

# Define comprehensive list of test files to validate
# We test as many types as possible to find edge cases in code generation
set(TEST_STUBS_TO_COMPILE
    # Basic types (should always work)
    floats
    integers
    strings
    char
    
    # Collections
    lists
    
    # Composite types
    records
    variants
    enums
    flags
    tuples
    
    # Option/Result types
    options
    results
    
    # Function features
    multi-return
    conventions
    
    # Resources (likely to expose issues)
    resources
    
    # Async features (likely to expose issues)
    # Note: These may fail - that's useful information!
    streams
    futures
)

# Remove the helper function - we want to try compiling even if files might not exist
# The build will fail if they don't, which tells us generation failed

# Collect source files that should exist after generation
set(STUB_SOURCES "")
set(STUB_HEADERS "")
foreach(stub_name ${TEST_STUBS_TO_COMPILE})
    # Add main header
    set(stub_header "${STUB_OUTPUT_DIR}/${stub_name}.hpp")
    list(APPEND STUB_HEADERS ${stub_header})
    
    # Add WAMR header
    set(stub_wamr_hpp "${STUB_OUTPUT_DIR}/${stub_name}_wamr.hpp")
    list(APPEND STUB_HEADERS ${stub_wamr_hpp})
    
    # Add WAMR implementation
    set(stub_wamr_cpp "${STUB_OUTPUT_DIR}/${stub_name}_wamr.cpp")
    list(APPEND STUB_SOURCES ${stub_wamr_cpp})
endforeach()

# Mark generated files as GENERATED property
set_source_files_properties(${STUB_SOURCES} ${STUB_HEADERS} PROPERTIES GENERATED TRUE)

# Create a compilation validation library
add_library(test-stubs-compiled STATIC EXCLUDE_FROM_ALL
    ${STUB_SOURCES}
)

# Link against cmcpp
target_link_libraries(test-stubs-compiled
    PRIVATE cmcpp
)

# Add include directories
target_include_directories(test-stubs-compiled PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${STUB_OUTPUT_DIR}
)

# Set C++ standard
target_compile_features(test-stubs-compiled PUBLIC cxx_std_20)

# Only suppress noise warnings, keep real errors visible
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(test-stubs-compiled PRIVATE
        -Wno-unused-parameter
        -Wno-unused-variable
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(test-stubs-compiled PRIVATE
        -Wno-unused-parameter
        -Wno-unused-variable  
    )
elseif(MSVC)
    target_compile_options(test-stubs-compiled PRIVATE
        /wd4100  # unreferenced formal parameter
        /wd4101  # unreferenced local variable
    )
endif()

# This library depends on stub generation
add_dependencies(test-stubs-compiled generate-test-stubs)

# ===== Target: validate-test-stubs =====
# Validates stub generation by attempting to compile them
add_custom_target(validate-test-stubs
    COMMAND ${CMAKE_COMMAND} -E echo "Validating generated stubs by compilation..."
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target test-stubs-compiled
    COMMENT "Compiling generated stubs to validate code generation"
    VERBATIM
)
add_dependencies(validate-test-stubs test-stubs-compiled)

# ===== Target: validate-test-stubs-basic =====
# Validate only basic types (should always pass)
add_custom_target(validate-test-stubs-basic
    COMMAND ${CMAKE_COMMAND} -E echo "Validating basic types..."
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/validate_stubs.py" 
        --group basic --verbose
        --stub-dir "${STUB_OUTPUT_DIR}"
        --include-dir "${CMAKE_SOURCE_DIR}/include"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS generate-test-stubs
    COMMENT "Validating basic type stubs"
    VERBATIM
)

# ===== Target: validate-test-stubs-composite =====
# Validate composite types (records, variants, enums, flags)
add_custom_target(validate-test-stubs-composite
    COMMAND ${CMAKE_COMMAND} -E echo "Validating composite types..."
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/validate_stubs.py" 
        --group composite --verbose
        --stub-dir "${STUB_OUTPUT_DIR}"
        --include-dir "${CMAKE_SOURCE_DIR}/include"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS generate-test-stubs
    COMMENT "Validating composite type stubs"
    VERBATIM
)

# ===== Target: validate-test-stubs-async =====
# Validate async types (streams, futures - likely to fail)
add_custom_target(validate-test-stubs-async
    COMMAND ${CMAKE_COMMAND} -E echo "Validating async types (streams, futures)..."
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/validate_stubs.py" 
        --group async --verbose
        --stub-dir "${STUB_OUTPUT_DIR}"
        --include-dir "${CMAKE_SOURCE_DIR}/include"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS generate-test-stubs
    COMMENT "Validating async type stubs"
    VERBATIM
)

# ===== Target: validate-test-stubs-incremental =====
# Validate all groups except async (for incremental testing)
add_custom_target(validate-test-stubs-incremental
    COMMAND ${CMAKE_COMMAND} -E echo "Validating all non-async stubs..."
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/validate_stubs.py" 
        --group all --exclude async --verbose
        --stub-dir "${STUB_OUTPUT_DIR}"
        --include-dir "${CMAKE_SOURCE_DIR}/include"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS generate-test-stubs
    COMMENT "Validating all stubs except async"
    VERBATIM
)

# ===== Target: test-stubs-full =====
# Combined target: generate stubs and validate them
add_custom_target(test-stubs-full
    DEPENDS validate-test-stubs
    COMMENT "Generate and validate WIT test stubs"
)

# ===== CTest Integration =====
# Add a test that runs the full stub generation and validation
add_test(
    NAME wit-stub-generation-test
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target test-stubs-full
)

set_tests_properties(wit-stub-generation-test PROPERTIES
    LABELS "codegen;stubs"
    TIMEOUT 300
)

# ===== Target: clean-test-stubs =====
# Clean generated stubs
add_custom_target(clean-test-stubs
    COMMAND ${CMAKE_COMMAND} -E echo "Cleaning generated test stubs..."
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${STUB_OUTPUT_DIR}"
    COMMENT "Removing generated stub files"
    VERBATIM
)

# ===== Summary =====
message(STATUS "Stub generation test targets configured:")
message(STATUS "  generate-test-stubs            - Generate C++ stubs from WIT test files")
message(STATUS "  validate-test-stubs            - Compile ALL stubs (full validation)")
message(STATUS "  validate-test-stubs-basic      - Compile only basic types (should pass)")
message(STATUS "  validate-test-stubs-composite  - Compile only composite types")
message(STATUS "  validate-test-stubs-async      - Compile only async types (likely fails)")
message(STATUS "  validate-test-stubs-incremental- Compile all except async")
message(STATUS "  test-stubs-full                - Generate + validate all")
message(STATUS "  clean-test-stubs               - Remove generated stubs")
message(STATUS "")
message(STATUS "Stub generation configuration:")
message(STATUS "  WIT test directory: ${WIT_TEST_DIR}")
message(STATUS "  Output directory:   ${STUB_OUTPUT_DIR}")
message(STATUS "  Python interpreter: ${Python3_EXECUTABLE}")
message(STATUS "  Test samples:       ${CMAKE_CURRENT_LIST_LENGTH} test files")
message(STATUS "")
message(STATUS "IMPORTANT: Compilation failures are EXPECTED and USEFUL!")
message(STATUS "  They indicate bugs in wit-codegen that need to be fixed.")
message(STATUS "  Check build output to see which WIT features cause issues.")
message(STATUS "")
message(STATUS "Incremental workflow:")
message(STATUS "  1. cmake --build build --target validate-test-stubs-basic")
message(STATUS "  2. cmake --build build --target validate-test-stubs-composite")
message(STATUS "  3. Fix any issues found")
message(STATUS "  4. cmake --build build --target validate-test-stubs-async")
message(STATUS "  5. Fix async issues")
message(STATUS "  6. cmake --build build --target validate-test-stubs (full test)")
