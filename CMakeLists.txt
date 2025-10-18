cmake_minimum_required(VERSION 3.10)

project(cmcpp 
    VERSION 0.1.0
    DESCRIPTION "C++20 header-only implementation of the WebAssembly Component Model canonical ABI"
    HOMEPAGE_URL "https://github.com/GordonSmith/component-model-cpp"
    LANGUAGES CXX
)

# ============================================================================
# CMake Module Path
# ============================================================================
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ============================================================================
# Build Options
# ============================================================================
# Determine default for BUILD_SAMPLES: OFF on Windows, ON elsewhere
option(BUILD_SAMPLES "Build samples" ON)
option(BUILD_TESTING "Build tests" ON)
option(BUILD_WIT_CODEGEN "Generate code from ANTLR grammar" ON)

# ============================================================================
# Core Library Target
# ============================================================================
add_library(cmcpp INTERFACE)

target_include_directories(cmcpp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(cmcpp INTERFACE cxx_std_20)

# ============================================================================
# Testing Setup
# ============================================================================
# Enable testing early so subdirectories (including samples) can register tests
if(BUILD_TESTING)
    enable_testing()
endif()

# ============================================================================
# Subdirectories
# ============================================================================
# Samples
if(BUILD_SAMPLES)
    add_subdirectory(samples)
endif()

# Grammar and code generation tools
# Grammar must be added before tools/test so the generate-grammar target is available
if(BUILD_WIT_CODEGEN)
    add_subdirectory(grammar)
    add_subdirectory(tools/wit-codegen)
endif()

# Tests
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# ============================================================================
# Installation
# ============================================================================
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

