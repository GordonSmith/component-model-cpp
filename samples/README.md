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
├── README.md           # This file
├── CMakeLists.txt      # Build configuration
├── wamr/               # WAMR runtime sample (fully implemented)
│   ├── main.cpp        # Sample entry point and implementation
│   ├── host_impl.cpp   # Host function implementations
│   └── generated/      # WIT-generated bindings (tracked in git)
│       ├── sample.hpp
│       ├── sample_wamr.hpp
│       └── sample_wamr.cpp
└── wasm/               # Guest WebAssembly modules
    ├── sample.wit      # WIT interface definition
    ├── main.cpp        # Guest implementation
    └── CMakeLists.txt  # Guest build configuration
```

## Samples

### WAMR Sample (Fully Implemented)

**Runtime**: [WAMR (WebAssembly Micro Runtime)](https://github.com/bytecodealliance/wasm-micro-runtime)

**Status**: ✅ Complete

The WAMR sample is a comprehensive demonstration using WIT-generated bindings for type-safe host integration.

**Features Demonstrated**:
- All Component Model primitive types (bool, integers, floats, char, strings)
- Composite types (tuples, records, lists)
- Advanced types (variants, options, results, enums, flags)
- String encoding conversions (UTF-8, UTF-16, Latin-1+UTF-16)
- Memory management and proper resource cleanup
- Error handling and debugging

**Quick Start**:
```bash
# Build the samples
cmake --preset linux-ninja-Debug -DBUILD_SAMPLES=ON
cmake --build --preset linux-ninja-Debug

# Run the sample
cd build/samples/wamr
./wamr
```

See [`wamr/README.md`](wamr/README.md) for detailed documentation, including:
- Complete API reference
- Type system examples
- Building and running instructions
- Troubleshooting guide

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
.\build\samples\wamr\wamr.exe
```

### Build Options

- `-DBUILD_SAMPLES=ON`: Enable sample builds (default: OFF)
- `-DWASI_SDK_PREFIX=/path`: Override WASI SDK location
- `-DBUILD_TESTS=ON`: Also build unit tests

## Guest WebAssembly Modules

The `wasm/` directory contains the guest-side WebAssembly component implementations:

### `sample.wit`
WIT interface definition that describes the host-guest contract:
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

### `main.cpp`
C++ implementation of the guest module that exports functions defined in the WIT file and imports host functions.

### Building Guest Modules

Guest modules are built automatically as part of the sample build process. To rebuild manually:

```bash
cd wasm
mkdir -p build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$WASI_SDK_PREFIX/share/cmake/wasi-sdk.cmake
cmake --build .
```

This produces `sample.wasm` which is then used by the host samples.

## Code Generation

The WAMR sample includes generated bindings in `wamr/generated/` that are **tracked in git** for convenience:

- `sample.hpp`: Type definitions and interfaces from WIT
- `sample_wamr.hpp`: WAMR-specific host function wrappers
- `sample_wamr.cpp`: Implementation of WAMR integration

These files are auto-generated from `wasm/sample.wit` using the `wit-codegen` tool:

```bash
# Regenerate bindings (if WIT changes)
cd build
./tools/wit-parser/wit-codegen \
  --wit ../../wasm/sample.wit \
  --output ../samples/wamr/generated
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
# Install via vcpkg
vcpkg install wasi-sdk

# Or download manually from:
# https://github.com/WebAssembly/wasi-sdk/releases
export WASI_SDK_PREFIX=/path/to/wasi-sdk
```

### "Cannot find wasm-tools or wit-bindgen-cli"

Install the required Rust tools:

```bash
cargo install wasm-tools wit-bindgen-cli
```

### Link Errors with WAMR

Ensure WAMR is properly built by vcpkg:

```bash
# Clean and rebuild
rm -rf build vcpkg_installed
cmake --preset linux-ninja-Debug -DBUILD_SAMPLES=ON
cmake --build --preset linux-ninja-Debug
```

### Runtime Errors

Enable verbose logging:
```cpp
// In your code
#define WAMR_VERBOSE 1
```

See individual sample READMEs for runtime-specific troubleshooting.

## Contributing

To add a new sample:

1. Create a directory under `samples/` for your runtime
2. Add CMake configuration in `samples/CMakeLists.txt`
3. Define WIT interfaces for your sample
4. Generate bindings using `wit-codegen`
5. Implement host interface classes
6. Add comprehensive README with usage instructions
7. Add tests demonstrating key functionality

See [`CONTRIBUTING.md`](../CONTRIBUTING.md) (if exists) for general contribution guidelines.

## References

- [Component Model Specification](https://github.com/WebAssembly/component-model)
- [Canonical ABI Documentation](https://github.com/WebAssembly/component-model/blob/main/design/mvp/CanonicalABI.md)
- [WIT Format](https://github.com/WebAssembly/component-model/blob/main/design/mvp/WIT.md)
- [WAMR Documentation](https://github.com/bytecodealliance/wasm-micro-runtime)

## License

See [LICENSE](../LICENSE) in the root directory.
