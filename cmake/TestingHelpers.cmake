# TestingHelpers.cmake
# Helper functions for setting up tests

# Function to add a grammar-based test
function(add_grammar_test)
    set(options "")
    set(oneValueArgs NAME SOURCE_FILE WORKING_DIR TIMEOUT)
    set(multiValueArgs DEPENDENCIES LABELS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    if(NOT BUILD_WIT_CODEGEN OR NOT TARGET generate-grammar)
        message(STATUS "Skipping test ${ARG_NAME}: Grammar generation not enabled")
        return()
    endif()
    
    # Find ANTLR4 runtime
    find_package(antlr4-runtime CONFIG QUIET)
    if(NOT antlr4-runtime_FOUND)
        message(WARNING "ANTLR4 runtime not found. Test ${ARG_NAME} will not be built.")
        return()
    endif()
    
    # Determine ANTLR4 library
    if(TARGET antlr4_shared)
        set(ANTLR4_LIBRARY antlr4_shared)
    elseif(TARGET antlr4_static)
        set(ANTLR4_LIBRARY antlr4_static)
    else()
        message(FATAL_ERROR "Neither antlr4_shared nor antlr4_static targets are available")
    endif()
    
    # ANTLR generated sources
    set(ANTLR_OUTPUT_DIR "${CMAKE_BINARY_DIR}/grammar/grammar")
    set(ANTLR_GENERATED_SOURCES
        "${ANTLR_OUTPUT_DIR}/WitLexer.cpp"
        "${ANTLR_OUTPUT_DIR}/WitParser.cpp"
        "${ANTLR_OUTPUT_DIR}/WitVisitor.cpp"
        "${ANTLR_OUTPUT_DIR}/WitBaseVisitor.cpp"
    )
    set_source_files_properties(${ANTLR_GENERATED_SOURCES} PROPERTIES GENERATED TRUE)
    
    # Create test executable
    add_executable(${ARG_NAME}
        ${ARG_SOURCE_FILE}
        ${ANTLR_GENERATED_SOURCES}
    )
    
    target_link_libraries(${ARG_NAME} PRIVATE ${ANTLR4_LIBRARY})
    target_compile_features(${ARG_NAME} PUBLIC cxx_std_17)
    
    target_include_directories(${ARG_NAME} PRIVATE
        ${CMAKE_BINARY_DIR}/grammar
        ${CMAKE_BINARY_DIR}/grammar/grammar
        ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include/antlr4-runtime
    )
    
    add_dependencies(${ARG_NAME} generate-grammar ${ARG_DEPENDENCIES})
    
    # Register test
    add_test(
        NAME ${ARG_NAME}
        COMMAND ${ARG_NAME}
        WORKING_DIRECTORY ${ARG_WORKING_DIR}
    )
    
    # Set properties
    if(ARG_TIMEOUT)
        set_tests_properties(${ARG_NAME} PROPERTIES TIMEOUT ${ARG_TIMEOUT})
    endif()
    
    if(ARG_LABELS)
        set_tests_properties(${ARG_NAME} PROPERTIES LABELS "${ARG_LABELS}")
    endif()
endfunction()

# Function to add a unit test with standard configuration
function(add_unit_test)
    set(options WITH_COVERAGE)
    set(oneValueArgs NAME)
    set(multiValueArgs SOURCES DEPENDENCIES LABELS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    add_executable(${ARG_NAME} ${ARG_SOURCES})
    
    target_link_libraries(${ARG_NAME} PRIVATE ${ARG_DEPENDENCIES})
    
    if(ARG_WITH_COVERAGE)
        add_coverage_support(${ARG_NAME})
    endif()
    
    add_test(
        NAME ${ARG_NAME}
        COMMAND $<TARGET_FILE:${ARG_NAME}>
    )
    
    if(ARG_LABELS)
        set_tests_properties(${ARG_NAME} PROPERTIES LABELS "${ARG_LABELS}")
    endif()
endfunction()
