# Packaging Guide for cmcpp

This document describes how to build, install, and distribute cmcpp packages.

## Overview

cmcpp uses CMake's built-in packaging capabilities:
- **CMake Config Files**: For integration with other CMake projects
- **CPack**: For creating distribution packages (tar.gz, DEB, RPM, ZIP, NSIS)

## Building and Installing

### Local Installation

Install cmcpp to a local prefix (default: `build/stage`):

```bash
cmake --preset linux-ninja-Debug
cmake --build build
cmake --build build --target install
```

The installation includes:
- **Headers**: All C++20 header files in `include/`
- **CMake Config**: Package config files for `find_package(cmcpp)`
- **Tools**: `wit-codegen` executable (if `BUILD_GRAMMAR=ON`)

### Custom Installation Prefix

```bash
cmake --preset linux-ninja-Debug -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build --target install
```

## Using Installed cmcpp in Other Projects

Once installed, other CMake projects can find and use cmcpp:

```cmake
cmake_minimum_required(VERSION 3.10)
project(MyProject)

# Find cmcpp
find_package(cmcpp REQUIRED)

# Create your executable
add_executable(my_app main.cpp)

# Link against cmcpp
target_link_libraries(my_app PRIVATE cmcpp::cmcpp)

# Optional: Use wit-codegen tool if available
if(DEFINED CMCPP_WIT_CODEGEN_EXECUTABLE)
    message(STATUS "wit-codegen found: ${CMCPP_WIT_CODEGEN_EXECUTABLE}")
endif()
```

Build your project with:

```bash
cmake . -DCMAKE_PREFIX_PATH=/path/to/cmcpp/install
cmake --build .
```

## Creating Distribution Packages

### Supported Package Formats

cmcpp supports multiple package formats via CPack:

- **Linux**: TGZ, DEB, RPM
- **Windows**: ZIP, NSIS
- **macOS**: TGZ, ZIP

### Generate All Default Packages

```bash
cd build
cpack
```

This generates packages for all default generators on your platform.

### Generate Specific Package Types

#### Tar.gz Archive (Cross-platform)

```bash
cd build
cpack -G TGZ
```

Output: `cmcpp-<version>-<platform>.tar.gz`

#### Debian Package (.deb)

```bash
cd build
cpack -G DEB
```

Output: `cmcpp_<version>_<arch>.deb`

Install on Ubuntu/Debian:
```bash
sudo dpkg -i cmcpp_0.1.0_amd64.deb
```

#### RPM Package (.rpm)

```bash
cd build
cpack -G RPM
```

Output: `cmcpp-<version>-<release>.<arch>.rpm`

Install on Red Hat/Fedora:
```bash
sudo rpm -i cmcpp-0.1.0-1.x86_64.rpm
```

#### Windows ZIP Archive

```bash
cd build
cpack -G ZIP
```

#### Windows NSIS Installer

```bash
cd build
cpack -G NSIS
```

Requires NSIS to be installed on Windows.

### Package Contents

All packages include:

1. **Headers**: Complete C++20 header-only library
   - Location: `include/cmcpp/`
   - Main header: `include/cmcpp.hpp`
   - Runtime integration: `include/wamr.hpp`
   - Boost.PFR headers: `include/boost/pfr/`

2. **CMake Integration Files**:
   - `lib/cmake/cmcpp/cmcppConfig.cmake` - Package configuration
   - `lib/cmake/cmcpp/cmcppConfigVersion.cmake` - Version compatibility
   - `lib/cmake/cmcpp/cmcppTargets.cmake` - Target exports

3. **Tools** (if `BUILD_GRAMMAR=ON`):
   - `bin/wit-codegen` - WIT code generator

### Package Metadata

Packages are configured with the following metadata:

- **Name**: cmcpp
- **Version**: From `PROJECT_VERSION` in CMakeLists.txt (e.g., 0.1.0)
- **Description**: C++20 WebAssembly Component Model implementation
- **License**: Apache-2.0 (from LICENSE file)
- **Homepage**: https://github.com/GordonSmith/component-model-cpp
- **Components**:
  - `headers` - C++ header files
  - `libraries` - CMake library targets
  - `tools` - wit-codegen command-line tool

## Version Compatibilitycmcpp uses semantic versioning with the following compatibility rules:

- **Pre-1.0 versions (0.x.y)**: Minor version must match exactly
  - `find_package(cmcpp 0.1)` only accepts 0.1.x versions
- **Post-1.0 versions (1.x.y+)**: Major version must match
  - `find_package(cmcpp 1.0)` accepts any 1.x.y version

This ensures API compatibility according to Component Model stability guarantees.

## Distribution

### GitHub Releases

Packages are automatically generated via CI/CD and attached to GitHub releases.

### vcpkg (Future)

A vcpkg port is planned for easier installation:

```bash
vcpkg install cmcpp
```

### Manual Distribution

To distribute cmcpp manually:

1. Build a release package:
   ```bash
   cmake --preset linux-ninja-Release -DCMAKE_INSTALL_PREFIX=/usr
   cmake --build build
   cd build && cpack -G TGZ
   ```

2. Distribute the generated `cmcpp-<version>-<platform>.tar.gz`

3. Users extract and set `CMAKE_PREFIX_PATH`:
   ```bash
   tar xzf cmcpp-0.1.0-Linux.tar.gz
   export CMAKE_PREFIX_PATH=$PWD/cmcpp-0.1.0-Linux
   ```

## Component Installation

CPack supports installing specific components:

```bash
# Install only headers (no tools)
cpack -G TGZ -D CPACK_COMPONENTS_ALL="headers;libraries"

# Install only tools
cpack -G TGZ -D CPACK_COMPONENTS_ALL="tools"
```

## Troubleshooting

### find_package(cmcpp) Not Found

Ensure `CMAKE_PREFIX_PATH` includes the installation directory:

```bash
cmake . -DCMAKE_PREFIX_PATH=/path/to/cmcpp/install
```

Or set the `cmcpp_DIR` variable directly:

```bash
cmake . -Dcmcpp_DIR=/path/to/cmcpp/install/lib/cmake/cmcpp
```

### Tools Not Included in Package

Make sure `BUILD_GRAMMAR=ON` when configuring:

```bash
cmake --preset linux-ninja-Debug -DBUILD_GRAMMAR=ON
```

### Package Generation Fails

Check CPack generators are available:

```bash
cpack --help | grep "Generators:"
```

Install required tools:
- **DEB**: `dpkg-deb` (usually pre-installed on Debian/Ubuntu)
- **RPM**: `rpmbuild` (`sudo apt-get install rpm` on Ubuntu)
- **NSIS**: Download from https://nsis.sourceforge.io/ (Windows only)

## Reference

- CMake Config File: `cmcppConfig.cmake.in`
- Version File: `cmcppConfigVersion.cmake.in`
- CPack Configuration: Bottom of root `CMakeLists.txt`
- Installation Rules: Root `CMakeLists.txt` (lines 60+)
