# Wasmtime Component Model Sample

This sample demonstrates how to use the Component Model C++ library with the [Wasmtime](https://github.com/bytecodealliance/wasmtime) WebAssembly runtime. It shows integration between C++ host code and WebAssembly guest modules using the Component Model's rich type system.

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   C++ Host      │    │   Component      │    │  WASM Guest     │
│   (wasmtime)    │◄──►│   Model ABI      │◄──►│   (sample.wasm) │
│                 │    │                  │    │                 │
└─────────────────┘    └──────────────────┘    └─────────────────┘
```

## Prerequisites

- **Wasmtime**: WebAssembly runtime (installed via vcpkg)
- **C++20**: Modern C++ compiler with C++20 support
- **CMake**: Build system
- **Component Model WASM**: The sample.wasm file (built from samples/wasm)

## Building

From the project root:

```bash
mkdir -p build
cd build
cmake ..
cmake --build . --target wasmtime --parallel
```

This will:
1. Build the WASM guest module (`samples/wasm` → `sample.wasm`)
2. Build the Wasmtime host application (`samples/wasmtime` → `wasmtime`)

## Running

```bash
cd build/samples/wasmtime
./wasmtime
```

### Expected Output

The sample produces well-organized output demonstrating each Component Model feature:

```
Wasmtime Component Model C++ Sample
====================================
Starting Wasmtime runtime initialization...
Wasmtime engine created successfully
Wasmtime store created successfully
Successfully loaded WASM file (104637 bytes)
Successfully compiled WASM module
Successfully instantiated WASM module
Successfully registered host functions

=== Testing Guest Functions ===

--- String Functions ---
call_reverse("Hello World!"): !DLROW OLLEH
call_reverse(call_reverse("Hello World!")): HELLO WORLD!

--- Variant Functions ---
variant_func((uint32_t)40): 80
variant_func((bool_t)true): 0
variant_func((bool_t)false): 1

--- Option Functions ---
option_func((uint32_t)40).has_value(): 1
option_func((uint32_t)40).value(): 80
option_func(std::nullopt).has_value(): 0

--- Void Functions ---
Hello, Void_Func!

--- Result Functions ---
ok_func result: 42
err_func result: error

--- Boolean Functions ---
call_and(false, false): 0
call_and(false, true): 0
call_and(true, false): 0
call_and(true, true): 1

--- Float Functions ---
call_add(3.1, 0.2): 3.3
call_add(1.5, 2.5): 4
call_add(DBL_MAX, 0.0): 8.98847e+307
call_add(DBL_MAX / 2, DBL_MAX / 2): 1.79769e+308

--- Complex String Functions ---
call_lots result: 42

--- Tuple Functions ---
call_reverse_tuple({false, "Hello World!"}): !DLROW OLLEH, 1

--- List Functions ---
call_list_filter result: 2

--- Enum Functions ---
enum_func(e::a): 0
enum_func(e::b): 1
enum_func(e::c): 2

=== Cleanup and Summary ===
Sample completed successfully!
```

## Code Structure

### Host Functions (C++ → WASM)

Host functions are C++ functions exported to the WebAssembly module:

```cpp
// Simple host function
void_t void_func() {
    std::cout << "Hello from host!" << std::endl;
}

// Host function with parameters and return value
cmcpp::bool_t and_func(cmcpp::bool_t a, cmcpp::bool_t b) {
    return a && b;
}

// Register host functions using Wasmtime linker
auto booleans_and_func = WASMTIME_HOST_FUNCTION(bool_t(bool_t, bool_t), store_ctx, and_func, lift_lower_context);
linker.define(store_ctx, "example:sample/booleans", "and", booleans_and_func);
```

### Guest Functions (WASM → C++)

Guest functions are WebAssembly functions called from C++ code:

```cpp
// Get function export from instance
auto reverse_export = instance.get_export(store_ctx, "example:sample/strings#reverse");
auto reverse_func_extern = reverse_export.ok().func();

// Create a callable wrapper for a WASM function
auto call_reverse = WASMTIME_GUEST_FUNCTION(string_t(string_t), store_ctx, *reverse_func_extern, lift_lower_context);

// Call the WASM function
auto result = call_reverse("Hello World!");
std::cout << "Result: " << result << std::endl;
```

### Component Model Types

The sample demonstrates all major Component Model types:

```cpp
// Primitive types
bool_t, uint32_t, float64_t, string_t

// Composite types
tuple_t<bool_t, string_t>
list_t<string_t>
variant_t<bool_t, uint32_t>
option_t<uint32_t>
result_t<uint32_t, string_t>
enum_t<MyEnum>
```

## Key Components

### 1. Runtime Initialization

```cpp
// Create Wasmtime configuration with component model support
WasmtimeConfig config;
config.wasm_component_model(true);
config.debug_info(true);

// Create engine and store
auto engine = wasmtime::Engine::create(config).ok();
auto store = wasmtime::Store<void>::create(engine);
auto store_ctx = store.context();
```

### 2. Module Compilation and Instantiation

```cpp
// Compile module from binary
auto module = wasmtime::Module::compile(engine, wasmtime::Span<uint8_t>(buffer)).ok();

// Create linker and instantiate
auto linker = wasmtime::Linker<void>::create(engine);
auto instance = linker.instantiate(store_ctx, module).ok();
```

### 3. Memory Management

```cpp
// Get memory export and create guest realloc wrapper
auto memory = instance.get_export(store_ctx, "memory").ok().memory().value();
auto realloc_func = instance.get_export(store_ctx, "cabi_realloc").ok().func().value();

WasmtimeGuestRealloc guest_realloc = [&store_ctx, realloc_func](uint32_t original_ptr, uint32_t original_size, 
                                                                uint32_t alignment, uint32_t new_size) -> uint32_t {
    std::vector<wasmtime::Val> args = {
        wasmtime::Val(static_cast<int32_t>(original_ptr)),
        wasmtime::Val(static_cast<int32_t>(original_size)),
        wasmtime::Val(static_cast<int32_t>(alignment)),
        wasmtime::Val(static_cast<int32_t>(new_size))
    };
    
    auto result = realloc_func.call(store_ctx, args).ok();
    return static_cast<uint32_t>(result[0].i32());
};
```

### 4. Context Setup

```cpp
// Create lift/lower context for type conversions
auto memory_data = memory.data(store_ctx);
auto memory_span = wasmtime::Span<uint8_t>(memory_data.data(), memory_data.size());
WasmtimeLiftLowerContext lift_lower_context(store_ctx, memory_span, guest_realloc);
```

### 5. Function Registration and Calling

```cpp
// Register host function
auto host_func = WASMTIME_HOST_FUNCTION(signature, store_ctx, cpp_function, lift_lower_context);
linker.define(store_ctx, "module", "function_name", host_func);

// Call guest function
auto guest_func = WASMTIME_GUEST_FUNCTION(signature, store_ctx, wasm_function, lift_lower_context);
auto result = guest_func(arguments...);
```

## Wasmtime vs WAMR Comparison

### Wasmtime Advantages
- **Native Component Model Support**: Built-in support for the Component Model specification
- **Advanced JIT Compilation**: Cranelift-based JIT compiler for high performance
- **Rich Configuration Options**: Extensive configuration for security, performance, and debugging
- **Modern C++ API**: Clean, RAII-based C++ wrapper API
- **Active Development**: Regularly updated with latest WebAssembly proposals

### WAMR Advantages
- **Smaller Footprint**: More compact runtime suitable for embedded systems
- **Multiple Execution Modes**: Interpreter, AOT, and JIT compilation options
- **Platform Support**: Broader platform and architecture support

### Key Differences in This Sample

**Wasmtime Approach:**
```cpp
// Configuration with component model support
WasmtimeConfig config;
config.wasm_component_model(true);

// Modern C++ API with Result types
auto engine_result = wasmtime::Engine::create(config);
if (!engine_result) {
    std::cerr << "Error: " << engine_result.err().message() << std::endl;
}
auto engine = engine_result.ok();
```

**WAMR Approach:**
```cpp
// C-style initialization
wasm_runtime_init();

// Manual error checking with error buffers
char error_buf[128];
module = wasm_runtime_load((uint8_t *)buffer, size, error_buf, sizeof(error_buf));
if (!module) {
    std::cerr << "Failed to load WASM module: " << error_buf << std::endl;
}
```

## Error Handling

The Wasmtime sample includes comprehensive error checking:
- Result type error handling throughout
- Exception-based error propagation
- Detailed error messages from Wasmtime
- Graceful cleanup on errors

## WebAssembly Interface Types (WIT)

The sample uses the same WIT definition as the WAMR sample (`samples/wasm/sample.wit`):

```wit
package example:sample;

interface booleans {
  and: func(a: bool, b: bool) -> bool;
}

interface strings {
  reverse: func(a: string) -> string;
  lots: func(p1: string, p2: string, /* ... */) -> u32;
}

// ... more interfaces

world sample {
  export booleans;
  export strings;
  // ... more exports
  
  import logging;
  import booleans;
  // ... more imports
}
```

## Performance Considerations

- **JIT Compilation**: Wasmtime's Cranelift JIT provides near-native performance
- **Component Model Overhead**: Type marshalling adds some overhead vs. raw WebAssembly
- **Memory Management**: Efficient memory allocation with proper cleanup
- **Zero-copy Operations**: String and binary data sharing where possible

## Troubleshooting

### Common Issues

1. **Wasmtime library not found**
   - Ensure Wasmtime is installed via vcpkg: `vcpkg install wasmtime-c-api wasmtime-cpp-api`
   - Check library paths in CMake output

2. **Component Model features not available**
   - Ensure component model is enabled: `config.wasm_component_model(true)`
   - Verify Wasmtime version supports component model

3. **Function export not found**
   - Verify function name matches WIT specification exactly
   - Check that the WASM module exports the function

4. **Memory access errors**
   - Ensure memory export is correctly retrieved
   - Verify memory span is properly initialized

### Debug Mode

Build with debug information for better error messages:

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --target wasmtime
```

## Advanced Features

### Component Model Integration

This sample showcases the Component Model's advanced features:

- **Interface Types**: Rich type system beyond basic WebAssembly types
- **Canonical ABI**: Proper implementation of the Component Model ABI
- **Memory Layout**: Correct handling of complex data structures
- **String Encodings**: Support for UTF-8, UTF-16, and Latin-1+UTF-16

### Wasmtime-Specific Features

- **Engine Configuration**: Extensive runtime configuration options
- **Linker**: Advanced module linking and import/export management
- **Memory Management**: Sophisticated memory allocation and management
- **Error Handling**: Comprehensive error reporting and recovery

## Build Scripts

For convenience, you can create build and run scripts:

**build.sh:**
```bash
#!/bin/bash
cd "$(dirname "$0")"
mkdir -p ../../build
cd ../../build
cmake ..
cmake --build . --target wasmtime --parallel
```

**run.sh:**
```bash
#!/bin/bash
cd "$(dirname "$0")"
cd ../../build/samples/wasmtime
./wasmtime
```

## Related Files

- **`main.cpp`**: Main sample application
- **`CMakeLists.txt`**: Build configuration
- **`../wasm/sample.wit`**: WebAssembly Interface Types definition
- **`../wasm/main.cpp`**: Guest-side implementation
- **`../../include/wasmtime.hpp`**: Wasmtime integration header
- **`../../include/cmcpp/`**: Component Model C++ library headers

## Further Reading

- [Wasmtime Documentation](https://docs.wasmtime.dev/)
- [WebAssembly Component Model Specification](https://github.com/WebAssembly/component-model)
- [Cranelift JIT Compiler](https://github.com/bytecodealliance/wasmtime/tree/main/cranelift)
- [Component Model C++ Library Documentation](../../README.md)

## Status

✅ **COMPLETED**: This sample is fully functional and demonstrates:

- **Wasmtime Integration**: Complete setup and initialization of Wasmtime engine, store, and linker
- **Host Function Registration**: All required host functions implemented with correct Component Model signatures
- **Function Signature Matching**: Proper Canonical ABI implementation for string functions
- **Bidirectional Communication**: Host-to-guest and guest-to-host function calls working
- **Type System Support**: Boolean, float64, string, and complex multi-parameter functions
- **Error Handling**: Comprehensive error reporting and resource management with RAII

### Working Features

- ✅ Boolean operations (`and` function with bool parameters)
- ✅ Float64 arithmetic (`add` function with f64 parameters)  
- ✅ String functions (`reverse` with Canonical ABI signatures)
- ✅ Complex functions (`lots` with 17 string parameters)
- ✅ Logging functions (`log-u32` with mixed parameter types)
- ✅ Automatic resource management with RAII wrappers
- ✅ Comprehensive error diagnostics and reporting

### Component Model ABI Implementation

The sample correctly implements the WebAssembly Component Model's Canonical ABI:

- **String Functions**: Use `(param i32 i32 i32)` signature where:
  - `i32`: String pointer in guest memory
  - `i32`: String length
  - `i32`: Realloc function for memory allocation
- **Complex Functions**: Simplified signatures for multi-parameter functions
- **Type Mapping**: Proper mapping between WIT types and Wasmtime C API types

## Comparison with WAMR Sample

| Feature | WAMR Sample | Wasmtime Sample |
|---------|-------------|-----------------|
| **Runtime** | WASM Micro Runtime | Wasmtime |
| **API Style** | High-level C++ wrappers | Direct C API with RAII |
| **Function Registration** | Automatic via `host_function()` | Manual via `wasmtime_linker_define_func()` |
| **Type Handling** | Transparent C++ types | Explicit Component Model ABI |
| **Memory Management** | Automatic | Manual with RAII wrappers |
| **Error Handling** | C++ exceptions | Wasmtime error types |
| **Performance** | Optimized for embedded | Optimized for server workloads |

### Key Implementation Differences

1. **Function Signatures**: Wasmtime requires explicit Canonical ABI signatures
2. **String Handling**: Manual implementation of Component Model string ABI
3. **Resource Management**: Custom RAII wrappers for Wasmtime objects
4. **Error Propagation**: Wasmtime-specific error handling patterns
