# ANTLR Grammar Code Generation
cmake_minimum_required(VERSION 3.10)

# Find ANTLR4 package from vcpkg (for C++ runtime, optional)
find_package(antlr4-runtime CONFIG QUIET)

# Find Java (ANTLR4 generator requires Java runtime)
find_package(Java COMPONENTS Runtime)
if(NOT Java_FOUND)
    message(WARNING "Java runtime not found. ANTLR4 requires Java to generate code.")
    return()
endif()

# Grammar and output directories
set(ANTLR_GRAMMAR_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "Directory containing grammar files")
set(ANTLR_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/grammar" CACHE PATH "Output directory for generated code")
set(ANTLR_VERSION "4.13.2" CACHE STRING "ANTLR version to use")
set(ANTLR_JAR_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../antlr-${ANTLR_VERSION}-complete.jar" CACHE FILEPATH "Path to ANTLR jar file")

# Find all .g4 grammar files
file(GLOB GRAMMAR_FILES "${ANTLR_GRAMMAR_DIR}/*.g4")

if(NOT GRAMMAR_FILES)
    message(WARNING "No .g4 grammar files found in ${ANTLR_GRAMMAR_DIR}")
    return()
endif()

# Create output directory
file(MAKE_DIRECTORY "${ANTLR_OUTPUT_DIR}")

# Download ANTLR jar if it doesn't exist
if(NOT EXISTS "${ANTLR_JAR_PATH}")
    set(ANTLR_URL "https://www.antlr.org/download/antlr-${ANTLR_VERSION}-complete.jar")
    message(STATUS "Downloading ANTLR ${ANTLR_VERSION} from ${ANTLR_URL}...")
    
    file(DOWNLOAD "${ANTLR_URL}" "${ANTLR_JAR_PATH}"
        STATUS download_status
        TIMEOUT 60
        SHOW_PROGRESS
    )
    
    list(GET download_status 0 status_code)
    list(GET download_status 1 status_string)
    
    if(NOT status_code EQUAL 0)
        message(WARNING "Failed to download ANTLR jar: ${status_string}")
        message(WARNING "Please manually download from ${ANTLR_URL} to ${ANTLR_JAR_PATH}")
        return()
    endif()
    
    message(STATUS "Successfully downloaded ANTLR jar to ${ANTLR_JAR_PATH}")
endif()

# Verify the jar file exists
if(NOT EXISTS "${ANTLR_JAR_PATH}")
    message(WARNING "ANTLR jar not found at ${ANTLR_JAR_PATH}")
    message(WARNING "Please download from https://www.antlr.org/download/antlr-${ANTLR_VERSION}-complete.jar")
    return()
endif()

# List of generated files (for dependency tracking)
set(GENERATED_FILES)
foreach(grammar_file ${GRAMMAR_FILES})
    get_filename_component(grammar_name ${grammar_file} NAME_WE)
    list(APPEND GENERATED_FILES
        "${ANTLR_OUTPUT_DIR}/${grammar_name}Lexer.h"
        "${ANTLR_OUTPUT_DIR}/${grammar_name}Lexer.cpp"
        "${ANTLR_OUTPUT_DIR}/${grammar_name}Parser.h"
        "${ANTLR_OUTPUT_DIR}/${grammar_name}Parser.cpp"
        "${ANTLR_OUTPUT_DIR}/${grammar_name}Visitor.h"
        "${ANTLR_OUTPUT_DIR}/${grammar_name}Visitor.cpp"
        "${ANTLR_OUTPUT_DIR}/${grammar_name}BaseVisitor.h"
        "${ANTLR_OUTPUT_DIR}/${grammar_name}BaseVisitor.cpp"
    )
endforeach()

# Create a custom command for grammar generation
add_custom_command(
    OUTPUT ${GENERATED_FILES}
    COMMAND ${Java_JAVA_EXECUTABLE}
        -jar "${ANTLR_JAR_PATH}"
        -Dlanguage=Cpp
        -o "${ANTLR_OUTPUT_DIR}"
        -visitor
        -no-listener
        -Xexact-output-dir
        ${GRAMMAR_FILES}
    WORKING_DIRECTORY "${ANTLR_GRAMMAR_DIR}"
    DEPENDS ${GRAMMAR_FILES}
    COMMENT "Generating ANTLR C++ grammar code from ${GRAMMAR_FILES}"
    VERBATIM
)

# Create a custom target that depends on the generated files
add_custom_target(generate-grammar
    DEPENDS ${GENERATED_FILES}
)


# Print information
message(STATUS "ANTLR Grammar Configuration:")
message(STATUS "  Java executable: ${Java_JAVA_EXECUTABLE}")
message(STATUS "  ANTLR version: ${ANTLR_VERSION}")
message(STATUS "  ANTLR jar: ${ANTLR_JAR_PATH}")
message(STATUS "  Grammar files: ${GRAMMAR_FILES}")
message(STATUS "  Output directory: ${ANTLR_OUTPUT_DIR}")
message(STATUS "  Generated files: ${GENERATED_FILES}")
message(STATUS "  Target: generate-grammar (run with 'cmake --build . --target generate-grammar')")
message(STATUS "  Note: Consumers must link to antlr4_shared and add ${ANTLR_OUTPUT_DIR} to includes")

