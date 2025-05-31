---
applyTo: 'samples/wamr/**'
---
# WAMR Instructions

## WebAssembly Micro Runtime Integration

This code demonstrates how to use the [WebAssembly Micro Runtime (WAMR)](https://github.com/bytecodealliance/wasm-micro-runtime) to host WebAssembly components with Component Model bindings.

### WAMR Overview
WAMR is a lightweight, high-performance WebAssembly runtime designed for:
- **Embedded Systems**: Optimized for resource-constrained environments
- **Edge Computing**: Fast startup and low memory footprint
- **Cross-platform**: Supports multiple architectures and operating systems
- **Multiple Execution Modes**: Interpreter, AOT, and JIT compilation

### Integration Architecture

#### Host Function Registration
```cpp
// Register native functions that the WebAssembly guest can call
bool success = wasm_runtime_register_natives_raw("module_name", symbols, count);
```

#### Component Model Host Functions
The `samples/wamr/main.cpp` demonstrates:
- **Type Mapping**: C++ types to Component Model types using `cmcpp` namespace
- **Function Wrappers**: Using `host_function()` to create WAMR-compatible function pointers
- **Module Organization**: Grouping functions by interface/module names

#### Memory Management
- **Linear Memory**: Access WebAssembly linear memory through `wasm_memory_get_base_address()`
- **Reallocation**: Use `cabi_realloc` for guest memory management
- **Memory Safety**: Proper bounds checking and address validation

### WAMR-Specific Implementation Details

#### Execution Environment
```cpp
wasm_exec_env_t exec_env = wasm_runtime_create_exec_env(module_inst, stack_size);
```
- **Stack Size**: Configure stack size for WebAssembly execution
- **Heap Size**: Set heap size for guest memory allocation
- **Module Instance**: Each module gets its own instance with isolated memory

#### Function Calling Convention
- **Native Symbols**: Use `NativeSymbol` arrays to define exported functions
- **Argument Passing**: Arguments passed via `uint64_t *args` array
- **Return Values**: Return values set through the same args array
- **Type Conversion**: Automatic conversion between C++ and WebAssembly types

#### Integration with Component Model
The `wamr.hpp` header provides:
- **Template Functions**: `export_func<F>()` for automatic type marshaling
- **Host Function Creation**: `host_function()` helper for creating native symbols
- **Memory Access**: Safe linear memory access with bounds checking
- **Encoding Support**: UTF-8, UTF-16 string encoding handling

### Sample Structure (`samples/wamr/`)

#### Files
- **main.cpp**: Complete example showing WAMR integration
- **CMakeLists.txt**: Build configuration for WAMR sample
- **main.c**: Alternative C implementation (if needed)

#### Example Functions Demonstrated
- **Void Functions**: Simple function calls without parameters
- **Boolean Operations**: Logical operations with Component Model bool_t
- **Numeric Operations**: Float and integer arithmetic
- **String Manipulation**: UTF-8 string processing and transformation
- **Logging**: Host-side logging from guest functions

### Build Requirements
- **WAMR Library**: Requires libiwasm and libvmlib
- **vcpkg Integration**: Uses vcpkg for dependency management
- **CMake Configuration**: Proper CMake setup for finding WAMR libraries

### Error Handling
- **Runtime Errors**: Check return values from WAMR API calls
- **Memory Errors**: Validate memory access and allocation
- **Type Errors**: Ensure proper type conversion between host and guest
- **Module Loading**: Handle WebAssembly module loading failures

### Performance Considerations
- **Function Call Overhead**: Minimize marshaling overhead for hot paths
- **Memory Allocation**: Efficient guest memory management
- **Type Conversions**: Optimize type lifting/lowering operations

### References
- [WAMR GitHub Repository](https://github.com/bytecodealliance/wasm-micro-runtime)
- [WAMR Documentation](https://github.com/bytecodealliance/wasm-micro-runtime/blob/main/doc/README.md)
- [WAMR Host API](https://github.com/bytecodealliance/wasm-micro-runtime/blob/main/core/iwasm/include/wasm_export.h)
