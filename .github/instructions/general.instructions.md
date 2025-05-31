---
applyTo: '**'
---
# General Instructions

## Modern C++ Project Guidelines

This repository is a modern C++20 header-only library implementing WebAssembly Component Model host bindings.

### C++ Standards and Features
- **C++20 Standard**: Use modern C++20 features throughout the codebase
- **Header-only Library**: All implementation is in the `include/` directory
- **Template-heavy Design**: Extensive use of templates for type safety and compile-time optimizations
- **Concepts**: Use C++20 concepts where appropriate for template constraints
- **constexpr**: Prefer constexpr for compile-time computations
- **RAII**: Follow RAII principles for resource management
- **Smart Pointers**: Use std::unique_ptr, std::shared_ptr instead of raw pointers
- **Standard Library**: Prefer STL containers and algorithms over custom implementations

### Code Organization (Reference: `include/` directory)
- **Namespace**: All code is in the `cmcpp` namespace
- **Modular Headers**: Each data type has its own header (integer.hpp, float.hpp, string.hpp, etc.)
- **Core Types**: Located in `include/cmcpp/` subdirectory
- **Integration Headers**: Top-level headers like `wamr.hpp` for runtime integration
- **Template Specializations**: Use template specialization for different WebAssembly types

### Build System
- **CMake**: Uses CMake 3.5+ with modern CMake practices
- **Interface Library**: Configured as an interface library target
- **Cross-platform**: Supports Ubuntu, macOS, and Windows
- **Compiler Requirements**: Requires C++20 support
- **Dependencies**: Uses vcpkg for third-party library management
  - Package definitions in `vcpkg.json` and `vcpkg-configuration.json`
  - Key dependencies: doctest (testing), ICU (Unicode), wasm-micro-runtime, wasi-sdk

### Type Safety and WebAssembly ABI
- **Strong Typing**: Use distinct types for WebAssembly values (bool_t, char_t, etc.)
- **ABI Compliance**: Implement proper lifting and lowering functions for WebAssembly Component Model
- **Memory Safety**: Handle WebAssembly linear memory access safely
- **Error Handling**: Use proper exception handling and validation

### Code Style Guidelines
- **Template Programming**: Follow modern template metaprogramming practices
- **Function Templates**: Use function templates for type-generic operations
- **SFINAE/Concepts**: Use concepts or SFINAE for template constraints
- **Const Correctness**: Maintain const correctness throughout
- **Naming**: Use snake_case for variables, PascalCase for types