# ============================================================================
# Unit Tests
# ============================================================================
project(unit-tests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include helper modules
include(CompilerOptions)
include(TestingHelpers)

# Find dependencies
find_package(doctest CONFIG REQUIRED)
find_package(ICU REQUIRED COMPONENTS uc dt in io)

# Source files location
set(TEST_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit-tests")

# ============================================================================
# Main Unit Test Executable
# ============================================================================
add_executable(${PROJECT_NAME} 
    ${TEST_SOURCE_DIR}/main.cpp
    ${TEST_SOURCE_DIR}/scratchpad.cpp
    ${TEST_SOURCE_DIR}/host-util.hpp
    ${TEST_SOURCE_DIR}/host-util.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../include
    ${ICU_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME} 
    PRIVATE doctest::doctest
    PRIVATE cmcpp
    PRIVATE ${ICU_LIBRARIES}
)

# Add coverage support
add_coverage_support(${PROJECT_NAME})
suppress_wasm_narrowing_warnings(${PROJECT_NAME})

# Register test
add_test(
    NAME ${PROJECT_NAME}
    COMMAND $<TARGET_FILE:${PROJECT_NAME}>
)

# ============================================================================
# Grammar Test Suite
# ============================================================================
# Validates the WIT grammar against wit-bindgen test files
add_grammar_test(
    NAME wit-grammar-test
    SOURCE_FILE ${TEST_SOURCE_DIR}/test_grammar.cpp
    WORKING_DIR ${CMAKE_SOURCE_DIR}
    TIMEOUT 120
    LABELS "grammar"
)

if(TARGET wit-grammar-test)
    target_compile_definitions(wit-grammar-test PRIVATE
        WIT_TEST_DIR="${CMAKE_SOURCE_DIR}/ref/wit-bindgen/tests/codegen"
    )
    
    message(STATUS "Grammar test configuration:")
    message(STATUS "  Test executable: wit-grammar-test")
    message(STATUS "  Test directory: ref/wit-bindgen/tests/codegen")
else()
    message(STATUS "Grammar library not built, skipping grammar tests")
    message(STATUS "  Enable with: cmake -DBUILD_WIT_CODEGEN=ON")
endif()

# ============================================================================
# WIT Stub Generation and Validation Tests
# ============================================================================
# Include the stub generation test module which generates and compiles stubs
# to validate the complete code generation pipeline
include(StubGenerationTests.cmake)

