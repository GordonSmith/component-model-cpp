cmake_minimum_required(VERSION 3.5...3.16)

project(cmcpp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost)

add_library(cmcpp STATIC
    src/context.cpp
    src/util.cpp
)

target_include_directories(cmcpp
    INTERFACE include
    PRIVATE ${Boost_INCLUDE_DIR}
)

target_link_libraries(cmcpp 
)

include_directories(
    include
)

if (BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")
    enable_testing()
    add_subdirectory(test)
endif()
