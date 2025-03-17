cmake_minimum_required(VERSION 3.5...3.16)

project(cmcpp LANGUAGES CXX)

find_package(Boost REQUIRED)

add_library(cmcpp INTERFACE)

target_include_directories(cmcpp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(cmcpp INTERFACE cxx_std_20)

if (BUILD_SAMPLES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/samples/CMakeLists.txt")
    set(WASI_SDK_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/x64-linux/wasi-sdk)
    add_subdirectory(samples)
endif()

if (BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")
    enable_testing()
    add_subdirectory(test)
endif()

include(GNUInstallDirs)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(TARGETS cmcpp
    EXPORT cmcppTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT cmcppTargets
    FILE cmcppTargets.cmake
    NAMESPACE cmcpp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cmcpp
)
