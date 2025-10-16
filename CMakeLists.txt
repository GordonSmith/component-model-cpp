cmake_minimum_required(VERSION 3.10)

project(cmcpp 
    VERSION 0.1.0
    DESCRIPTION "C++20 header-only implementation of the WebAssembly Component Model canonical ABI"
    HOMEPAGE_URL "https://github.com/GordonSmith/component-model-cpp"
    LANGUAGES CXX
)

# Determine default for BUILD_SAMPLES: OFF on Windows, ON elsewhere
set(_BUILD_SAMPLES_DEFAULT ON)
if (WIN32)
    set(_BUILD_SAMPLES_DEFAULT ON)
endif()
option(BUILD_SAMPLES "Build samples" ${_BUILD_SAMPLES_DEFAULT})
option(BUILD_TESTING "Build tests" ON)
option(WIT_CODEGEN "Generate code from ANTLR grammar" ON)

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

# Grammar must be added before tools/test so the generate-grammar target is available
if (WIT_CODEGEN AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/grammar/CMakeLists.txt")
    add_subdirectory(grammar)
    # Add tools that use the grammar
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/wit-codegen/CMakeLists.txt")
        add_subdirectory(tools/wit-codegen)
    endif()
endif()

if (BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")
    enable_testing()
    add_subdirectory(test)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install header files
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install library target
install(TARGETS cmcpp
    EXPORT cmcppTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install CMake export file
install(EXPORT cmcppTargets
    FILE cmcppTargets.cmake
    NAMESPACE cmcpp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cmcpp
)

# Generate and install CMake config files
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmcppConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmcppConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cmcpp
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_BINDIR
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/cmcppConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/cmcppConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/cmcppConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cmcpp
)

# Install wit-codegen tool if it was built
if(WIT_CODEGEN AND TARGET wit-codegen)
    install(TARGETS wit-codegen
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT tools
    )
    
    # Install ANTLR4 runtime DLL on Windows
    if(WIN32)
        # Find and install the ANTLR4 runtime DLL
        set(ANTLR4_DLL_PATH "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/bin/antlr4-runtime.dll")
        if(EXISTS "${ANTLR4_DLL_PATH}")
            install(FILES "${ANTLR4_DLL_PATH}"
                DESTINATION ${CMAKE_INSTALL_BINDIR}
                COMPONENT tools
            )
            message(STATUS "Will install ANTLR4 runtime DLL: ${ANTLR4_DLL_PATH}")
        else()
            message(WARNING "ANTLR4 runtime DLL not found at: ${ANTLR4_DLL_PATH}")
        endif()
    elseif(UNIX AND TARGET antlr4_shared)
        # On Linux/macOS, try to find and install the shared library
        if(APPLE)
            set(ANTLR4_LIB_PATTERN "libantlr4-runtime*.dylib")
        else()
            set(ANTLR4_LIB_PATTERN "libantlr4-runtime.so*")
        endif()
        
        file(GLOB ANTLR4_LIBS "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib/${ANTLR4_LIB_PATTERN}")
        if(ANTLR4_LIBS)
            install(FILES ${ANTLR4_LIBS}
                DESTINATION ${CMAKE_INSTALL_LIBDIR}
                COMPONENT tools
            )
        endif()
    endif()
endif()

# CPack configuration for package generation
set(CPACK_PACKAGE_NAME "cmcpp")
set(CPACK_PACKAGE_VENDOR "GordonSmith")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "C++20 WebAssembly Component Model implementation")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/GordonSmith/component-model-cpp")
set(CPACK_PACKAGE_CONTACT "GordonSmith")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

# Package components
set(CPACK_COMPONENTS_ALL libraries headers tools)
set(CPACK_COMPONENT_LIBRARIES_DISPLAY_NAME "CMake Library Targets")
set(CPACK_COMPONENT_HEADERS_DISPLAY_NAME "C++ Header Files")
set(CPACK_COMPONENT_TOOLS_DISPLAY_NAME "WIT Code Generator Tool")
set(CPACK_COMPONENT_HEADERS_DESCRIPTION "Header-only C++20 Component Model implementation")
set(CPACK_COMPONENT_TOOLS_DESCRIPTION "wit-codegen executable for generating C++ bindings from WIT files")

# Platform-specific package generators
if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "cmcpp ${PROJECT_VERSION}")
    set(CPACK_NSIS_PACKAGE_NAME "cmcpp")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
elseif(APPLE)
    set(CPACK_GENERATOR "TGZ;ZIP")
elseif(UNIX)
    set(CPACK_GENERATOR "TGZ;DEB;RPM")
    
    # Debian/Ubuntu package settings
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "GordonSmith")
    set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    
    # RPM package settings
    set(CPACK_RPM_PACKAGE_LICENSE "Apache-2.0")
    set(CPACK_RPM_PACKAGE_GROUP "Development/Libraries")
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
endif()

# Source package
set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")
set(CPACK_SOURCE_IGNORE_FILES
    /.git
    /.github
    /build
    /\\\\.vscode
    /\\\\.vs
    /\\\\.cache
    \\\\.gitignore
    \\\\.gitmodules
)

include(CPack)

