cmake_minimum_required(VERSION 3.10)

project(cmcpp LANGUAGES CXX)

option(BUILD_SAMPLES "Build samples" ON)
option(BUILD_TESTING "Build tests" ON)

add_library(cmcpp INTERFACE)

target_include_directories(cmcpp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(cmcpp INTERFACE cxx_std_20)

if (BUILD_SAMPLES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/samples/CMakeLists.txt")
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(WASI_SDK_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/x64-windows/wasi-sdk)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64")
            set(WASI_SDK_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/arm64-osx/wasi-sdk)
        else()
            set(WASI_SDK_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/x64-osx/wasi-sdk)
        endif()
    else()
        set(WASI_SDK_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/x64-linux/wasi-sdk)
    endif()
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
