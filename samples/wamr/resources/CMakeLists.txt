cmake_minimum_required(VERSION 3.16)
project(resource-examples)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(ICU REQUIRED COMPONENTS uc dt in io)

# Include directories
include_directories(
    ${CMAKE_CURRENT_LIST_DIR}/../../../include
    ${ICU_INCLUDE_DIR}
)

# Simple resource demonstration
add_executable(simple-resource-demo
    simple_resource_demo.cpp
    ../../../test/host-util.hpp
    ../../../test/host-util.cpp
)

target_link_libraries(simple-resource-demo
    PRIVATE cmcpp
    PRIVATE ${ICU_LIBRARIES}
)

# Full resource examples (with potential WAMR integration)
add_executable(resource-examples
    resource_examples.cpp
    ../../../test/host-util.hpp
    ../../../test/host-util.cpp
)

target_link_libraries(resource-examples
    PRIVATE cmcpp
    PRIVATE ${ICU_LIBRARIES}
)

# Install targets
install(TARGETS simple-resource-demo resource-examples
    RUNTIME DESTINATION bin
)
