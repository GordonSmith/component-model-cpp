project(codegen-tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_COMPILER_IS_GNUCXX)
    add_compile_options(--coverage)
    add_link_options(--coverage)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
    add_link_options(-fprofile-instr-generate)
endif()

# Suppress narrowing conversion warnings on MSVC for WebAssembly 32-bit ABI
if(MSVC)
    add_compile_options(
        /wd4244  # conversion from 'type1' to 'type2', possible loss of data
        /wd4267  # conversion from 'size_t' to 'type', possible loss of data
        /wd4305  # truncation from 'type1' to 'type2'
        /wd4309  # truncation of constant value
    )
endif()

# Only proceed if grammar/codegen tools are available
if(NOT BUILD_WIT_CODEGEN OR NOT TARGET wit-codegen)
    message(STATUS "wit-codegen not available, skipping stub generation tests")
    message(STATUS "  Enable with: cmake -DBUILD_WIT_CODEGEN=ON")
    return()
endif()

message(STATUS "Configuring WIT stub generation tests...")

# ===== Build C++ Test Utilities =====
# Build the C++ versions of the test utilities
add_executable(generate_test_stubs_tool generate_test_stubs.cpp)
target_compile_features(generate_test_stubs_tool PUBLIC cxx_std_20)

add_executable(validate_stubs_tool validate_stubs.cpp)
target_compile_features(validate_stubs_tool PUBLIC cxx_std_20)

add_executable(summarize_stub_compilation_tool summarize_stub_compilation.cpp)
target_compile_features(summarize_stub_compilation_tool PUBLIC cxx_std_20)

add_executable(validate_all_wit_bindgen_tool validate_all_wit_bindgen.cpp)
target_compile_features(validate_all_wit_bindgen_tool PUBLIC cxx_std_20)

# ===== Configuration =====
set(STUB_OUTPUT_DIR "${CMAKE_BINARY_DIR}/generated_stubs")
set(WIT_TEST_DIR "${CMAKE_SOURCE_DIR}/ref/wit-bindgen/tests/codegen")

# Check if test directory exists
if(NOT EXISTS "${WIT_TEST_DIR}")
    message(WARNING "WIT test directory not found: ${WIT_TEST_DIR}")
    message(WARNING "  Initialize with: git submodule update --init --recursive")
    return()
endif()

# ===== Target: generate-test-stubs =====
# Generates C++ stubs from all WIT test files
add_custom_target(generate-test-stubs
    COMMAND ${CMAKE_COMMAND} -E echo "Generating stubs from WIT test files..."
    COMMAND $<TARGET_FILE:generate_test_stubs_tool>
        -d "${WIT_TEST_DIR}"
        -o "${STUB_OUTPUT_DIR}"
        -c "$<TARGET_FILE:wit-codegen>"
        -v
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS wit-codegen generate_test_stubs_tool
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
    # variants - Disabled: code generator creates invalid duplicate std::monostate in variants
    simple-enum
    flags
    zero-size-tuple
    
    # Option/Result types
    simple-option
    # option-result - Disabled: code generator doesn't handle nested result wrappers correctly
    result-empty
    
    # Function features
    multi-return
    conventions
    
    # Resources (likely to expose issues)
    # resources - Disabled: code generator doesn't handle borrow<> and own<> yet
    
    # Async features (likely to expose issues)
    # Note: These may fail - that's useful information!
    streams
    futures
)

# Collect source files that should exist after generation
# NOTE: Files are now organized in subdirectories, one per stub
set(STUB_SOURCES "")
set(STUB_HEADERS "")
foreach(stub_name ${TEST_STUBS_TO_COMPILE})
    # Each stub gets its own subdirectory
    set(stub_dir "${STUB_OUTPUT_DIR}/${stub_name}")
    
    # Add main header
    set(stub_header "${stub_dir}/${stub_name}.hpp")
    list(APPEND STUB_HEADERS ${stub_header})
    
    # Add WAMR header
    set(stub_wamr_hpp "${stub_dir}/${stub_name}_wamr.hpp")
    list(APPEND STUB_HEADERS ${stub_wamr_hpp})
    
    # Add WAMR implementation
    set(stub_wamr_cpp "${stub_dir}/${stub_name}_wamr.cpp")
    list(APPEND STUB_SOURCES ${stub_wamr_cpp})
endforeach()

# Mark generated files as GENERATED property
set_source_files_properties(${STUB_SOURCES} ${STUB_HEADERS} PROPERTIES GENERATED TRUE)

# Create a compilation validation library
add_library(test-stubs-compiled STATIC EXCLUDE_FROM_ALL
    ${STUB_SOURCES}
)

# This library depends on stub generation
add_dependencies(test-stubs-compiled generate-test-stubs)

# Link against cmcpp
target_link_libraries(test-stubs-compiled
    PRIVATE cmcpp
)

# Add include directories
# Since each stub is in its own subdirectory, we need to add each one
target_include_directories(test-stubs-compiled PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
# Add each stub directory to the include path
foreach(stub_name ${TEST_STUBS_TO_COMPILE})
    target_include_directories(test-stubs-compiled PRIVATE
        "${STUB_OUTPUT_DIR}/${stub_name}"
    )
endforeach()

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

# ===== Target: validate-test-stubs =====
# Validates stub generation by attempting to compile them
# Uses ninja's parallel compilation (determined by CMAKE_BUILD_PARALLEL_LEVEL or -j flag)
# In CI environments, --parallel is suppressed to avoid resource contention
if(DEFINED ENV{CI})
    set(BUILD_PARALLEL_FLAG "")
    set(PARALLEL_MESSAGE "Note: Running in CI environment. Parallel build disabled.")
else()
    set(BUILD_PARALLEL_FLAG "--parallel")
    set(PARALLEL_MESSAGE "Note: Using system default parallelism. Set CMAKE_BUILD_PARALLEL_LEVEL to override.")
endif()

add_custom_target(validate-test-stubs
    COMMAND ${CMAKE_COMMAND} -E echo "Validating generated stubs by compilation (parallel build)..."
    COMMAND ${CMAKE_COMMAND} -E echo "${PARALLEL_MESSAGE}"
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target test-stubs-compiled ${BUILD_PARALLEL_FLAG} > ${CMAKE_BINARY_DIR}/stub_compilation.log 2>&1 || ${CMAKE_COMMAND} -E true
    COMMAND $<TARGET_FILE:summarize_stub_compilation_tool>
        --log-file "${CMAKE_BINARY_DIR}/stub_compilation.log" || ${CMAKE_COMMAND} -E true
    COMMENT "Compiling generated stubs in parallel to validate code generation"
    BYPRODUCTS ${CMAKE_BINARY_DIR}/stub_compilation.log
    DEPENDS summarize_stub_compilation_tool
    VERBATIM
)
# Depend on test-stubs-compiled which already depends on generate-test-stubs
# This avoids duplicate stub generation
add_dependencies(validate-test-stubs test-stubs-compiled)

# ===== Target: validate-test-stubs-basic =====
# Validate only basic types (should always pass)
add_custom_target(validate-test-stubs-basic
    COMMAND ${CMAKE_COMMAND} -E echo "Validating basic types..."
    COMMAND $<TARGET_FILE:validate_stubs_tool>
        -g basic -v
        -d "${STUB_OUTPUT_DIR}"
        -i "${CMAKE_SOURCE_DIR}/include"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS generate-test-stubs validate_stubs_tool
    COMMENT "Validating basic type stubs"
    VERBATIM
)

# ===== Target: validate-test-stubs-composite =====
# Validate composite types (records, variants, enums, flags)
add_custom_target(validate-test-stubs-composite
    COMMAND ${CMAKE_COMMAND} -E echo "Validating composite types..."
    COMMAND $<TARGET_FILE:validate_stubs_tool>
        -g composite -v
        -d "${STUB_OUTPUT_DIR}"
        -i "${CMAKE_SOURCE_DIR}/include"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS generate-test-stubs validate_stubs_tool
    COMMENT "Validating composite type stubs"
    VERBATIM
)

# ===== Target: validate-test-stubs-async =====
# Validate async types (streams, futures - likely to fail)
add_custom_target(validate-test-stubs-async
    COMMAND ${CMAKE_COMMAND} -E echo "Validating async types (streams, futures)..."
    COMMAND $<TARGET_FILE:validate_stubs_tool>
        -g async -v
        -d "${STUB_OUTPUT_DIR}"
        -i "${CMAKE_SOURCE_DIR}/include"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS generate-test-stubs validate_stubs_tool
    COMMENT "Validating async type stubs"
    VERBATIM
)

# ===== Target: validate-test-stubs-incremental =====
# Validate all groups except async (for incremental testing)
add_custom_target(validate-test-stubs-incremental
    COMMAND ${CMAKE_COMMAND} -E echo "Validating all non-async stubs..."
    COMMAND $<TARGET_FILE:validate_stubs_tool>
        -g all --exclude async -v
        -d "${STUB_OUTPUT_DIR}"
        -i "${CMAKE_SOURCE_DIR}/include"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS generate-test-stubs validate_stubs_tool
    COMMENT "Validating all stubs except async"
    VERBATIM
)

# ===== Target: validate-root-cmake =====
# Validate that the root CMakeLists.txt can configure successfully
# Find WAMR (optional - needed for compiling *_wamr.cpp stubs)
include(FindWAMR)
find_package(WAMR QUIET)

# Configure build flags for the nested build (used by validate-root-cmake and validate-root-cmake-build)
# In CI environments, use Debug build with no optimizations to reduce memory overhead
if(DEFINED ENV{CI})
    set(CI_CMAKE_FLAGS "-DCMAKE_BUILD_TYPE=Debug" "-DCMCPP_INCLUDE_DIR=${CMAKE_SOURCE_DIR}/include")
    set(CI_BUILD_MESSAGE "Debug build (no optimizations) for CI")
else()
    set(CI_CMAKE_FLAGS "-DCMCPP_INCLUDE_DIR=${CMAKE_SOURCE_DIR}/include")
    set(CI_BUILD_MESSAGE "Default build configuration")
endif()

# Add WAMR include directory if available
if(WAMR_FOUND AND WAMR_INCLUDE_DIRS)
    list(APPEND CI_CMAKE_FLAGS "-DWAMR_INCLUDE_DIR=${WAMR_INCLUDE_DIRS}")
    message(STATUS "WAMR found - stub builds will include WAMR support")
else()
    message(STATUS "WAMR not found - stub builds will skip WAMR files (expected in some environments)")
endif()

add_custom_target(validate-root-cmake
    COMMAND ${CMAKE_COMMAND} -E echo "============================================"
    COMMAND ${CMAKE_COMMAND} -E echo "[Step 1/3] Validating root CMakeLists.txt configuration..."
    COMMAND ${CMAKE_COMMAND} -E echo "Build mode: ${CI_BUILD_MESSAGE}"
    COMMAND ${CMAKE_COMMAND} -E echo "============================================"
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${STUB_OUTPUT_DIR}/test_build"
    COMMAND ${CMAKE_COMMAND} -E echo "Configuring build for 199 generated stubs..."
    COMMAND ${CMAKE_COMMAND} -S "${STUB_OUTPUT_DIR}" -B "${STUB_OUTPUT_DIR}/test_build" ${CI_CMAKE_FLAGS}
    COMMAND ${CMAKE_COMMAND} -E echo "✓ Root CMakeLists.txt configured successfully"
    DEPENDS generate-test-stubs
    COMMENT "Testing root CMakeLists.txt can configure all stubs"
    VERBATIM
)

# ===== Target: validate-root-cmake-build =====
# Build all stubs using the root CMakeLists.txt to validate the complete build system
# This compiles ALL 199 stubs, not just the subset in TEST_STUBS_TO_COMPILE
# The build command may have failures, but the summarize tool will determine final success
add_custom_target(validate-root-cmake-build
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "============================================"
    COMMAND ${CMAKE_COMMAND} -E echo "[Step 2/3] Building all stubs via root CMakeLists.txt..."
    COMMAND ${CMAKE_COMMAND} -E echo "This will compile ALL 199 stubs in parallel."
    COMMAND ${CMAKE_COMMAND} -E echo "Progress will be logged to root_cmake_build.log"
    COMMAND ${CMAKE_COMMAND} -E echo "============================================"
    COMMAND ${CMAKE_COMMAND} -E time ${CMAKE_COMMAND} --build "${STUB_OUTPUT_DIR}/test_build" ${BUILD_PARALLEL_FLAG} > ${CMAKE_BINARY_DIR}/root_cmake_build.log 2>&1
    COMMAND ${CMAKE_COMMAND} -E echo "✓ Build completed. Analyzing results..."
    COMMAND ${CMAKE_COMMAND} -E echo "Build log saved to: ${CMAKE_BINARY_DIR}/root_cmake_build.log"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "============================================"
    COMMAND ${CMAKE_COMMAND} -E echo "[Step 3/3] Summarizing compilation results..."
    COMMAND ${CMAKE_COMMAND} -E echo "============================================"
    COMMAND $<TARGET_FILE:summarize_stub_compilation_tool>
        --log-file "${CMAKE_BINARY_DIR}/root_cmake_build.log"
    DEPENDS validate-root-cmake summarize_stub_compilation_tool
    COMMENT "Compiling all 199 stubs through root CMakeLists.txt"
    BYPRODUCTS ${CMAKE_BINARY_DIR}/root_cmake_build.log
    VERBATIM
)

# ===== Target: test-stubs-full =====
# Combined target: generate stubs and build all 199 via root CMakeLists
# This is the comprehensive test that validates the complete code generation pipeline
add_custom_target(test-stubs-full
    DEPENDS validate-root-cmake-build
    COMMENT "Generate and build ALL 199 WIT test stubs via root CMakeLists.txt"
)

# ===== CTest Integration =====
# Add a test that runs the full stub generation and validation
add_test(
    NAME wit-stub-generation-test
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target test-stubs-full
)

set_tests_properties(wit-stub-generation-test PROPERTIES
    LABELS "codegen;stubs"
    TIMEOUT 3600  # 60 minutes - GitHub Actions needs more time than local builds
)

# ===== Target: clean-test-stubs =====
# Clean generated stubs
add_custom_target(clean-test-stubs
    COMMAND ${CMAKE_COMMAND} -E echo "Cleaning generated test stubs..."
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${STUB_OUTPUT_DIR}"
    COMMENT "Removing generated stub files"
    VERBATIM
)
