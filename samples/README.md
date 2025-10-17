# Component Model C++ Samples

This directory contains sample implementations demonstrating how to use the Component Model C++ library with different WebAssembly runtimes.

## Overview

The samples showcase real-world integration between C++ host applications and WebAssembly Component Model guests, demonstrating:

- **Host-Guest Communication**: Bidirectional function calls between C++ and WebAssembly
- **Type System**: Complete coverage of Component Model types (primitives, composites, resources)
- **Runtime Integration**: Working with different WebAssembly runtimes
- **Code Generation**: WIT-generated bindings for type-safe host integration
- **Memory Management**: Proper handling of linear memory, string encodings, and resource lifetimes

## Directory Structure

```
samples/
├── README.md                 # This file
├── CMakeLists.txt            # Top-level build orchestration
├── CMAKE_REFACTORING.md      # CMake refactoring documentation
├── wasm/                     # WebAssembly guest modules
│   ├── CMakeLists.txt        # Guest build configuration
│   └── sample-one/           # Individual guest sample
│       ├── CMakeLists.txt    # Sample-specific build
│       ├── sample.wit        # WIT interface definition
│       └── main.cpp          # Guest implementation
└── wamr/                     # WAMR host applications
    ├── CMakeLists.txt        # Host build configuration  
    └── sample-one/           # Individual host sample
        ├── CMakeLists.txt    # Sample-specific build
        ├── main.cpp          # Host application entry
        ├── host_impl.cpp     # Host function implementations
        └── generated/        # WIT-generated bindings (tracked in git)
            ├── sample.hpp
            ├── sample_wamr.hpp
            └── sample_wamr.cpp
```

**Key Organization Principles:**
- Each sample has matching directories under both `wasm/` and `wamr/`
- Guest modules build with WASI SDK toolchain (isolated via ExternalProject)
- Host applications link against WAMR runtime and cmcpp library
- Generated bindings are sample-specific and committed to git for convenience

## Samples

### Sample One (WAMR)

**Location**: `wasm/sample-one/` and `wamr/sample-one/`

**Runtime**: [WAMR (WebAssembly Micro Runtime)](https://github.com/bytecodealliance/wasm-micro-runtime)

**Status**: ✅ Complete

A comprehensive demonstration using WIT-generated bindings for type-safe host integration.

**Features Demonstrated**:
- All Component Model primitive types (bool, integers, floats, char, strings)
- Composite types (tuples, records, lists)
- Advanced types (variants, options, results, enums, flags)
- String encoding conversions (UTF-8, UTF-16, Latin-1+UTF-16)
- Memory management and proper resource cleanup
- Error handling and debugging

**Quick Start**:
```bash
# Build the samples (both guest and host)
cmake --preset linux-ninja-Debug -DBUILD_SAMPLES=ON
cmake --build --preset linux-ninja-Debug

# Run the WAMR host executable
./build/samples/wamr/sample-one/sample-one

# Or use the target name
cmake --build --preset linux-ninja-Debug --target sample-one
./build/samples/wamr/sample-one/sample-one
```

**Output**: The guest WebAssembly component is built at:
- Core module: `build/samples/wasm/sample-one/sample.wasm`
- Component: `build/samples/wasm/sample-one/sample-component.wasm`

See [Generated WAMR Helpers](../docs/generated-wamr-helpers.md) for detailed API documentation.

### WasmTime Sample (Planned)

**Runtime**: [WasmTime](https://github.com/bytecodealliance/wasmtime)

**Status**: 🚧 Planned

Future sample demonstrating integration with the WasmTime runtime, including:
- Component Model native support
- Async/streaming APIs
- Resource management
- WASI support

### WasmEdge Sample (Planned)

**Runtime**: [WasmEdge](https://github.com/WasmEdge/WasmEdge)

**Status**: 🚧 Planned

Future sample showcasing WasmEdge integration with:
- AI/ML extensions
- Networking capabilities
- Edge computing scenarios

### Wasmer Sample (Planned)

**Runtime**: [Wasmer](https://github.com/wasmerio/wasmer)

**Status**: 🚧 Planned

Future sample demonstrating Wasmer integration.

## Building Samples

### Prerequisites

1. **Install dependencies** via vcpkg (handled automatically by CMake presets)
2. **Install WASI SDK** for guest module compilation:
   ```bash
   # Via vcpkg (recommended)
   vcpkg install wasi-sdk
   
   # Or set manually
   export WASI_SDK_PREFIX=/path/to/wasi-sdk
   ```
3. **Install Rust tools** for WIT processing:
   ```bash
   cargo install wasm-tools wit-bindgen-cli
   ```

### Build Commands

**Linux/macOS**:
```bash
# Configure with samples enabled
cmake --preset linux-ninja-Debug -DBUILD_SAMPLES=ON

# Build
cmake --build --preset linux-ninja-Debug

# Run WAMR sample
./build/samples/wamr/wamr
```

**Windows**:
```bash
# Configure with samples enabled
cmake --preset windows-ninja-Debug -DBUILD_SAMPLES=ON

# Build
cmake --build --preset windows-ninja-Debug

# Run WAMR sample
.\build\samples\wamr\sample-one\sample-one.exe
```

## Build System Architecture

The samples use a modular CMake architecture optimized for Component Model development:

### Three-Layer Structure

1. **Top Level** (`samples/CMakeLists.txt`)
   - Tool discovery (Ninja, wit-bindgen, wasm-tools)
   - ExternalProject setup for guest builds
   - Target orchestration (`samples`, `wasm-guests`, `wamr-samples`)

2. **Runtime Level** (`wamr/CMakeLists.txt`, potentially `wasmtime/`, etc.)
   - Runtime-specific discovery (WAMR libraries, headers)
   - Shared configuration via INTERFACE libraries (`wamr-common`)
   - Platform-specific compiler options

3. **Sample Level** (e.g., `wamr/sample-one/CMakeLists.txt`)
   - WIT file and binding generation
   - Sample-specific source files
   - Links against runtime and cmcpp libraries

### Key Design Decisions

**ExternalProject for Guests**: WebAssembly guest modules are built as external projects to isolate the WASI SDK toolchain from the host build environment.

**INTERFACE Libraries**: The `wamr-common` library provides shared WAMR configuration (headers, libraries, compiler flags) that all WAMR samples can link against.

**Pre-Generated Bindings**: Generated C++ host bindings are committed to git for convenience and faster builds when wit-codegen is unavailable.

**Graceful Degradation**: Missing tools (wasm-tools, wit-bindgen) cause warnings but don't fail the build completely.

See [CMAKE_REFACTORING.md](CMAKE_REFACTORING.md) for complete architectural documentation and rationale.

### Build Options

- `-DBUILD_SAMPLES=ON`: Enable sample builds (default: ON on Linux/macOS, ON on Windows)
- `-DWASI_SDK_PREFIX=/path`: Override WASI SDK location
- `-DBUILD_TESTING=ON`: Also build unit tests (default: ON)
- `-DWIT_CODEGEN=ON`: Enable wit-codegen tool build (default: ON)

### Build Targets

The refactored build system provides multiple targets for flexibility:

**All Samples**:
```bash
cmake --build build --target samples
# Builds: wasm-guests + wamr-samples
```

**Guest Modules Only**:
```bash
cmake --build build --target wasm-guests
# Builds: build/samples/wasm/sample-one/sample-component.wasm
```

**Host Applications Only**:
```bash
cmake --build build --target wamr-samples
# Builds: build/samples/wamr/sample-one/sample-one
```

**Individual Sample**:
```bash
cmake --build build --target sample-one
# Builds specific WAMR host executable
```

See [CMAKE_REFACTORING.md](CMAKE_REFACTORING.md) for detailed build system documentation.

## Guest WebAssembly Modules

The `wasm/` directory contains guest-side WebAssembly component implementations. Each sample has its own subdirectory.

### Sample Structure

For each sample (e.g., `sample-one`):

**`sample.wit`** - WIT interface definition describing the host-guest contract:
```wit
package example:sample;

interface booleans {
  and: func(a: bool, b: bool) -> bool;
}

interface floats {
  add: func(a: f64, b: f64) -> f64;
}

// ... more interfaces
```

**`main.cpp`** - C++ implementation that:
- Imports host functions (defined in WIT)
- Exports guest functions (defined in WIT)
- Uses generated C bindings from `wit-bindgen`

**`CMakeLists.txt`** - Build configuration that:
- Generates C bindings using `wit-bindgen`
- Compiles to core WebAssembly module (`.wasm`)
- Converts to component using `wasm-tools`

### Building Guest Modules

Guest modules are built automatically as part of the sample build via CMake's ExternalProject:

```bash
# Automatic build (recommended)
cmake --build build --target wasm-guests

# Manual build (for debugging)
cd build/samples/wasm-prefix/src/wasm-build/sample-one
ninja
```

This produces:
- `sample.wasm` - Core WebAssembly module
- `sample-component.wasm` - Component Model wrapper (if wasm-tools is available)

### Guest Build Environment

Guest builds use an isolated WASI SDK environment:
- **Toolchain**: WASI SDK (via `CMAKE_TOOLCHAIN_FILE`)
- **Generator**: Ninja (for fast, parallel builds)
- **Sysroot**: WASI sysroot from SDK
- **Linker flags**: `--no-entry` (no main function), `--export-dynamic`

## Code Generation

Each sample includes generated bindings in its `generated/` directory, **tracked in git** for convenience.

### Host Bindings (C++)

Generated from WIT using the `wit-codegen` tool:

**Location**: `wamr/sample-one/generated/`

**Files**:
- `sample.hpp`: Type definitions, interfaces, and function declarations
- `sample_wamr.hpp`: WAMR-specific integration helpers
- `sample_wamr.cpp`: WAMR native symbol registration arrays

### Guest Bindings (C)

Generated from WIT using `wit-bindgen`:

**Location**: `build/samples/wasm-prefix/src/wasm-build/sample-one/sample/` (build-time)

**Files**:
- `sample.h`: C header with type definitions and function declarations
- `sample.c`: C implementation stubs and marshaling code
- `sample_component_type.o`: Component type metadata

### Regenerating Bindings

If you modify a WIT file, regenerate bindings:

**Host bindings** (requires `wit-codegen` tool):
```bash
# Build wit-codegen first
cmake --build build --target wit-codegen

# Generate host bindings
cd samples/wamr/sample-one/generated
../../../../build/tools/wit-codegen/wit-codegen \
  ../../../wasm/sample-one/sample.wit
```

**Guest bindings** (automatic on rebuild):
```bash
# Just rebuild - bindings regenerate automatically
cmake --build build --target wasm-guests
```

## Usage Patterns

### Host Function Registration

```cpp
#include "generated/sample_wamr.hpp"

// Implement interface - bindings auto-generated from WIT
class MyHostImpl : public sample::host::logger {
  void log(cmcpp::LiftLowerContext& cx, cmcpp::string_t msg) override {
    std::cout << msg << std::endl;
  }
};
```

### Guest Function Invocation

```cpp
auto booleans = sample::guest::booleans::create(module, instance);
bool result = booleans.and_(true, false);  // Type-safe call
```

## Implementation Approach

The samples use **WIT-generated bindings** for type-safe host integration:

**Advantages**:
- Type-safe, idiomatic C++ API
- Automatic marshaling and memory management
- WIT-driven development workflow
- Less boilerplate code
- Compile-time interface validation

**Requirements**:
- WIT interface definitions (`.wit` files)
- `wit-codegen` tool for code generation
- Component Model-compliant WebAssembly runtime

**Development Workflow**:
1. Define interfaces in WIT format
2. Generate C++ bindings using `wit-codegen`
3. Implement host interface classes
4. Build and link guest WebAssembly modules
5. Run and test the integrated application

## Testing

Each sample includes its own test suite demonstrating correct behavior:

```bash
# Build and run all tests
cmake --preset linux-ninja-Debug -DBUILD_SAMPLES=ON -DBUILD_TESTS=ON
cmake --build --preset linux-ninja-Debug
ctest --test-dir build --output-on-failure
```

## Troubleshooting

### "WASI_SDK_PREFIX is not set"

The guest modules require WASI SDK for compilation:

```bash
# Install via vcpkg (recommended)
vcpkg install wasi-sdk

# WASI_SDK_PREFIX is auto-detected from vcpkg installation
# Manual override (if needed):
cmake --preset linux-ninja-Debug -DWASI_SDK_PREFIX=/path/to/wasi-sdk
```

### "Cannot find wasm-tools or wit-bindgen-cli"

Install the required Rust tools:

```bash
cargo install wasm-tools wit-bindgen-cli

# Verify installation
which wasm-tools
which wit-bindgen
```

### "Ninja build tool not found"

```bash
# Install via vcpkg
./vcpkg/vcpkg fetch ninja

# Or install system-wide
# Ubuntu/Debian:
sudo apt-get install ninja-build

# macOS:
brew install ninja

# Windows:
choco install ninja
```

### "WAMR runtime not found"

Ensure WAMR is properly built by vcpkg:

```bash
# Clean and rebuild
rm -rf build vcpkg_installed
cmake --preset linux-ninja-Debug -DBUILD_SAMPLES=ON
cmake --build --preset linux-ninja-Debug

# Verify WAMR installation
find build/vcpkg_installed -name "libiwasm*"
```

### Link Errors with WAMR

Check that you're linking against `wamr-common`:

```cmake
# In your sample CMakeLists.txt
target_link_libraries(my-sample PRIVATE
    cmcpp::cmcpp
    wamr-common    # Provides WAMR headers and libraries
)
```

### Guest Build Fails

Check the external project logs:

```bash
# View configuration log
cat build/samples/wasm-prefix/src/wasm-stamp/wasm-configure-*.log

# View build log
cat build/samples/wasm-prefix/src/wasm-stamp/wasm-build-*.log

# Rebuild with verbose output
cmake --build build --target wasm-guests -- -v
```

### Runtime Errors

Enable WAMR logging in your host code:

```cpp
// Before wasm_runtime_full_init
RuntimeInitArgs init_args;
memset(&init_args, 0, sizeof(RuntimeInitArgs));
init_args.mem_alloc_type = Alloc_With_System_Allocator;

// Enable logging
char global_heap_buf[512 * 1024];
init_args.mem_alloc_option.pool.heap_buf = global_heap_buf;
init_args.mem_alloc_option.pool.heap_size = sizeof(global_heap_buf);

wasm_runtime_full_init(&init_args);
```

See individual sample READMEs for sample-specific troubleshooting.

## Contributing

To add a new sample:

### 1. Create Guest Sample

```bash
mkdir -p samples/wasm/my-sample
cd samples/wasm/my-sample
```

Create `my-sample.wit`, `main.cpp`, and `CMakeLists.txt` following the pattern in `sample-one/`.

Add to `samples/wasm/CMakeLists.txt`:
```cmake
add_subdirectory(my-sample)
```

### 2. Create Host Sample

```bash
mkdir -p samples/wamr/my-sample
cd samples/wamr/my-sample
```

Create `main.cpp`, `host_impl.cpp`, and `CMakeLists.txt` following the pattern in `sample-one/`.

Add to `samples/wamr/CMakeLists.txt`:
```cmake
add_subdirectory(my-sample)

# Update wamr-samples target
add_custom_target(wamr-samples)
add_dependencies(wamr-samples sample-one my-sample)
```

### 3. Generate Bindings

```bash
# Build wit-codegen
cmake --build build --target wit-codegen

# Generate host bindings
mkdir -p samples/wamr/my-sample/generated
cd samples/wamr/my-sample/generated
../../../../build/tools/wit-codegen/wit-codegen \
  ../../../wasm/my-sample/my-sample.wit

# Commit generated files
git add .
```

### 4. Build and Test

```bash
cmake --build build --target my-sample
./build/samples/wamr/my-sample/my-sample
```

### 5. Document

Add a section to this README describing your sample.

See [CMAKE_REFACTORING.md](CMAKE_REFACTORING.md) for detailed examples and best practices.

## References

- [Component Model Specification](https://github.com/WebAssembly/component-model)
- [Canonical ABI Documentation](https://github.com/WebAssembly/component-model/blob/main/design/mvp/CanonicalABI.md)
- [WIT Format](https://github.com/WebAssembly/component-model/blob/main/design/mvp/WIT.md)
- [WAMR Documentation](https://github.com/bytecodealliance/wasm-micro-runtime)

## License

See the project [LICENSE](https://github.com/GordonSmith/component-model-cpp/blob/main/LICENSE) in the root directory.
