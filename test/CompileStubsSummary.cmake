# CMake script to compile stubs individually and provide a summary
# Usage: cmake -P CompileStubsSummary.cmake

cmake_minimum_required(VERSION 3.22)

# Get parameters from environment or command line
if(NOT DEFINED STUB_FILES)
    message(FATAL_ERROR "STUB_FILES must be defined")
endif()

if(NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "OUTPUT_DIR must be defined")
endif()

if(NOT DEFINED INCLUDE_DIRS)
    message(FATAL_ERROR "INCLUDE_DIRS must be defined")
endif()

if(NOT DEFINED CXX_COMPILER)
    message(FATAL_ERROR "CXX_COMPILER must be defined")
endif()

if(NOT DEFINED CXX_FLAGS)
    set(CXX_FLAGS "")
endif()

# Parse the semicolon-separated lists
string(REPLACE ";" " " STUB_FILES_LIST "${STUB_FILES}")
string(REPLACE ";" " -I" INCLUDE_DIRS_LIST "${INCLUDE_DIRS}")
set(INCLUDE_DIRS_LIST "-I${INCLUDE_DIRS_LIST}")

# Split stub files into a list
string(REPLACE " " ";" FILE_LIST "${STUB_FILES_LIST}")

set(SUCCESS_COUNT 0)
set(FAILURE_COUNT 0)
set(FAILED_FILES "")

message("================================================================================")
message("Compiling Generated Stubs - Individual File Validation")
message("================================================================================")
message("")

# Try to compile each file individually
foreach(stub_file ${FILE_LIST})
    get_filename_component(stub_name "${stub_file}" NAME_WE)
    
    # Try to compile
    execute_process(
        COMMAND ${CXX_COMPILER} ${CXX_FLAGS} ${INCLUDE_DIRS_LIST} -c "${stub_file}" -o "${OUTPUT_DIR}/${stub_name}.o"
        RESULT_VARIABLE compile_result
        OUTPUT_VARIABLE compile_output
        ERROR_VARIABLE compile_error
        WORKING_DIRECTORY ${OUTPUT_DIR}
    )
    
    if(compile_result EQUAL 0)
        message("[✓] ${stub_name}")
        math(EXPR SUCCESS_COUNT "${SUCCESS_COUNT} + 1")
    else()
        message("[✗] ${stub_name}")
        math(EXPR FAILURE_COUNT "${FAILURE_COUNT} + 1")
        list(APPEND FAILED_FILES "${stub_name}")
    endif()
endforeach()

# Print summary
message("")
message("================================================================================")
message("Compilation Summary")
message("================================================================================")
message("Total files:      ${CMAKE_CURRENT_LIST_LENGTH}")
message("Successful:       ${SUCCESS_COUNT}")
message("Failed:           ${FAILURE_COUNT}")

if(FAILURE_COUNT GREATER 0)
    message("")
    message("Failed files:")
    foreach(failed_file ${FAILED_FILES})
        message("  - ${failed_file}")
    endforeach()
    message("")
    message("To see detailed errors for a specific file, run:")
    message("  ninja test/CMakeFiles/test-stubs-compiled.dir/generated_stubs/<filename>_wamr.cpp.o")
endif()

message("================================================================================")
message("")

# Return non-zero if any failed
if(FAILURE_COUNT GREATER 0)
    message(FATAL_ERROR "Some stub files failed to compile")
endif()
