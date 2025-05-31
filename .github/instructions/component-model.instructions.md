---
applyTo: '**'
---
# Component Model Instructions

## Repository Goal

This repository implements a C++ ABI (Application Binary Interface) for the WebAssembly Component Model, providing host bindings that allow C++ applications to interact with WebAssembly components.

### WebAssembly Component Model Overview
The [WebAssembly Component Model](https://github.com/WebAssembly/component-model) is a specification that extends WebAssembly with:
- **Interface Types**: Rich type system beyond basic WebAssembly types
- **Component Linking**: Composable components with well-defined interfaces
- **World Definitions**: Complete application interface descriptions
- **WIT (WebAssembly Interface Types)**: IDL for describing component interfaces

### This Repository's Implementation

#### Core Features Implemented
- **Host Data Types**: Complete mapping of Component Model types to C++ types
  - Primitive types: bool, integers (s8-s64, u8-u64), floats (f32, f64), char
  - String types: UTF-8, UTF-16, Latin-1+UTF-16 encoding support  
  - Composite types: lists, records, tuples, variants, enums, options, results, flags
  - Resource types: own, borrow (planned)

- **ABI Functions**: Core Component Model operations
  - `lower_flat_values`: Convert C++ values to WebAssembly flat representation
  - `lift_flat_values`: Convert WebAssembly flat values to C++ types
  - Memory management with proper encoding handling

#### Architecture
- **Header-only Library**: Pure template-based implementation in `include/cmcpp/`
- **Type Safety**: Strong typing ensures correct Component Model ABI compliance
- **Runtime Integration**: Support for multiple WebAssembly runtimes
  - [WAMR (WebAssembly Micro Runtime)](https://github.com/bytecodealliance/wasm-micro-runtime) - implemented
  - [WasmTime](https://github.com/bytecodealliance/wasmtime) - planned
  - [WasmEdge](https://github.com/WasmEdge/WasmEdge) - planned
  - [Wasmer](https://github.com/wasmerio/wasmer) - planned

#### Component Model Compliance
- **Canonical ABI**: Implements the official Component Model Canonical ABI
- **Interface Types**: Full support for Interface Types specification
- **Memory Layout**: Proper handling of WebAssembly linear memory layout
- **Encoding**: Support for UTF-8, UTF-16, and Latin-1+UTF-16 string encodings
- **Alignment**: Correct alignment handling for different data types

### Usage Patterns
When working with this library:
1. **Define Interfaces**: Use C++ types that map to Component Model interface types
2. **Host Functions**: Implement host functions using the provided type wrappers
3. **Guest Interaction**: Use lift/lower functions to marshal data between host and guest
4. **Memory Management**: Use the provided realloc functions for guest memory allocation

### References
- [WebAssembly Component Model Specification](https://github.com/WebAssembly/component-model)
- [Component Model Canonical ABI](https://github.com/WebAssembly/component-model/blob/main/design/mvp/CanonicalABI.md)
- [WIT (WebAssembly Interface Types)](https://github.com/WebAssembly/component-model/blob/main/design/mvp/WIT.md)