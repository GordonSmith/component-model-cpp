project(component-model-cpp-tests)

option(ENABLE_TEST_COVERAGE "Enable test coverage" OFF)
option(TEST_INSTALLED_VERSION "Test the version found by find_package" OFF)

find_package(doctest CONFIG REQUIRED)
find_path(WASMTIME_CPP_API_INCLUDE_DIRS "wasmtime-cpp-api/wasmtime.hh"
  PATHS ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}
)
if (WIN32)
  find_library(WASMTIME_LIB NAMES wasmtime.dll
    PATHS ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}
  )
else()
  find_library(WASMTIME_LIB NAMES wasmtime
    PATHS ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}
  )
endif()

include_directories(
    ../src
    ${WASMTIME_CPP_API_INCLUDE_DIRS}/wasmtime-c-api
    ${WASMTIME_CPP_API_INCLUDE_DIRS}/wasmtime-cpp-api
)
file(GLOB sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_executable(${PROJECT_NAME} ${sources})
target_link_libraries(${PROJECT_NAME} 
  ${WASMTIME_LIB}
  doctest::doctest 
  component-model-cpp
)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20)
add_test(
  NAME ${PROJECT_NAME}
  COMMAND ${PROJECT_NAME}
  WORKING_DIRECTORY ${ROOT_DIR}
)

if(NOT TEST_INSTALLED_VERSION)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # target_compile_options(component-model-cpp PUBLIC -Wall -Wpedantic -Wextra -Werror -Wno-error=unused-parameter -Wno-unused-parameter)
  elseif(MSVC)
    target_compile_options(component-model-cpp PUBLIC /W4 /WX)
    target_compile_definitions(${PROJECT_NAME} PUBLIC DOCTEST_CONFIG_USE_STD_HEADERS)
  endif()
endif()

enable_testing()

include(${doctest_DIR}/doctest.cmake)
doctest_discover_tests(${PROJECT_NAME})

if(ENABLE_TEST_COVERAGE)
  target_compile_options(component-model-cpp PUBLIC -O0 -g -fprofile-arcs -ftest-coverage)
  target_link_options(component-model-cpp PUBLIC -fprofile-arcs -ftest-coverage)
endif()
