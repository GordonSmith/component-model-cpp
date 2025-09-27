cmake_minimum_required(VERSION 3.10)

project(cmcpp LANGUAGES CXX)

# Determine default for BUILD_SAMPLES: OFF on Windows, ON elsewhere
set(_BUILD_SAMPLES_DEFAULT ON)
if (WIN32)
    set(_BUILD_SAMPLES_DEFAULT ON)
endif()
option(BUILD_SAMPLES "Build samples" ${_BUILD_SAMPLES_DEFAULT})
option(BUILD_TESTING "Build tests" ON)

add_library(cmcpp INTERFACE)

target_include_directories(cmcpp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(cmcpp INTERFACE cxx_std_20)

if (BUILD_SAMPLES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/samples/CMakeLists.txt")
    if (NOT DEFINED WASI_SDK_PREFIX OR WASI_SDK_PREFIX STREQUAL "")
        set(_wasi_sdk_candidates)
        if (DEFINED VCPKG_TARGET_TRIPLET)
            list(APPEND _wasi_sdk_candidates "${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/wasi-sdk")
        endif()
        file(GLOB _wasi_sdk_glob "${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/*/wasi-sdk")
        list(APPEND _wasi_sdk_candidates ${_wasi_sdk_glob})
        list(FILTER _wasi_sdk_candidates EXCLUDE REGEX "^$")
        list(REMOVE_DUPLICATES _wasi_sdk_candidates)
        if (_wasi_sdk_candidates)
            list(GET _wasi_sdk_candidates 0 _wasi_sdk_default)
            set(WASI_SDK_PREFIX "${_wasi_sdk_default}" CACHE PATH "Path to the WASI SDK used by the samples" FORCE)
        else()
            set(WASI_SDK_PREFIX "" CACHE PATH "Path to the WASI SDK used by the samples" FORCE)
        endif()
    endif()
    message ("WASI_SDK_PREFIX: ${WASI_SDK_PREFIX}")
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
