# WAMR Component Model Sample

This sample demonstrates how to use the Component Model C++ library with WAMR (WebAssembly Micro Runtime) to interact between C++ host code and WebAssembly guest modules.

## Overview

The sample showcases:
- **Host Function Registration**: Exporting C++ functions to WebAssembly modules
- **Guest Function Invocation**: Calling WebAssembly functions from C++ code
- **Component Model Types**: Working with all major Component Model types including:
  - Primitive types (bool, integers, floats, strings)
  - Composite types (tuples, records, lists)
  - Advanced types (variants, options, results, enums, flags)
- **Memory Management**: Proper handling of WebAssembly linear memory
- **Error Handling**: Comprehensive error checking and resource cleanup

## Architecture

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   C++ Host      │    │  Component Model │    │  WASM Guest     │
│   Application   │◄──►│  C++ Library     │◄──►│  Module         │
│                 │    │                  │    │                 │
│ • Host funcs    │    │ • Type mapping   │    │ • Guest funcs   │
│ • Memory mgmt   │    │ • ABI handling   │    │ • Exports       │
│ • Error handling│    │ • Serialization  │    │ • Imports       │
└─────────────────┘    └──────────────────┘    └─────────────────┘
```

## Prerequisites

- **WAMR**: WebAssembly Micro Runtime (installed via vcpkg)
- **C++20**: Modern C++ compiler with C++20 support
- **CMake**: Build system
- **Component Model WASM**: The sample.wasm file (built from samples/wasm)

## Building

From the project root:

```bash
mkdir -p build
cd build
cmake ..
cmake --build . --target wamr --parallel
```

This will:
1. Build the WASM guest module (`samples/wasm` → `sample.wasm`)
2. Build the WAMR host application (`samples/wamr` → `wamr`)

## Running

```bash
cd build/samples/wamr
./wamr
```

### Expected Output

The sample produces well-organized output demonstrating each Component Model feature:

```
WAMR Component Model C++ Sample
===============================
Starting WAMR runtime initialization...
WAMR runtime initialized successfully
Successfully loaded WASM file (104637 bytes)
Successfully loaded WASM module
Successfully instantiated WASM module

=== Testing Guest Functions ===

--- String Functions ---
call_reverse("Hello World!"): !DLROW OLLEH
call_reverse(call_reverse("Hello World!")): HELLO WORLD!

--- Variant Functions ---
variant_func((uint32_t)40)80
variant_func((bool_t)true)0
variant_func((bool_t)false)1

--- Option Functions ---
option_func((uint32_t)40).has_value()1
option_func((uint32_t)40).value()80
option_func(std::nullopt).has_value()0

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

// Register host functions
NativeSymbol booleans_symbol[] = {
    host_function("and", and_func),
};
wasm_runtime_register_natives_raw("example:sample/booleans", 
                                  booleans_symbol, 
                                  sizeof(booleans_symbol) / sizeof(NativeSymbol));
```

### Guest Functions (WASM → C++)

Guest functions are WebAssembly functions called from C++ code:

```cpp
// Create a callable wrapper for a WASM function
auto call_reverse = guest_function<string_t(string_t)>(
    module_inst, exec_env, liftLowerContext,
    "example:sample/strings#reverse"
);

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
// Initialize WAMR runtime
wasm_runtime_init();

// Load and instantiate module
module = wasm_runtime_load(buffer, size, error_buf, sizeof(error_buf));
module_inst = wasm_runtime_instantiate(module, stack_size, heap_size, 
                                       error_buf, sizeof(error_buf));
```

### 2. Memory Management

```cpp
// Set up guest memory reallocation function
GuestRealloc realloc = [exec_env, cabi_realloc](int original_ptr, 
                                                int original_size, 
                                                int alignment, 
                                                int new_size) -> int {
    uint32_t argv[4] = {original_ptr, original_size, alignment, new_size};
    wasm_runtime_call_wasm(exec_env, cabi_realloc, 4, argv);
    return argv[0];
};
```

### 3. Context Setup

```cpp
// Create lift/lower context for type conversions
LiftLowerOptions opts(Encoding::Utf8, memory_span, realloc);
LiftLowerContext liftLowerContext(trap, convert, opts);
```

### 4. Error Handling

The sample includes comprehensive error checking:
- File I/O errors
- WAMR initialization failures
- Module loading/instantiation errors
- Function lookup failures
- Runtime execution errors

## WebAssembly Interface Types (WIT)

The sample uses the following WIT definition (`samples/wasm/sample.wit`):

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

- **Memory Management**: The sample uses efficient memory allocation with proper cleanup
- **Type Safety**: Strong typing prevents ABI mismatches at compile time
- **Zero-copy**: String and binary data can be shared without copying when possible
- **Flat Values**: Simple types are passed directly without heap allocation

## Troubleshooting

### Common Issues

1. **WASM file not found**
   - Ensure `sample.wasm` is built: `cmake --build . --target wasm`
   - Check file path in error message

2. **Library linking errors**
   - Verify WAMR is installed via vcpkg
   - Check CMake finds the libraries correctly

3. **Runtime crashes**
   - Enable debug symbols: `CMAKE_BUILD_TYPE=Debug`
   - Check WAMR error messages in `error_buf`

4. **Function not found**
   - Verify function name matches WIT specification exactly
   - Check exports in the WASM module

### Debug Mode

Build with debug information for better error messages:

```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --target wamr
```

## Recent Improvements

This sample has been enhanced with:

### Code Quality
- **Comprehensive Error Handling**: Added proper error checking for all WAMR operations
- **Resource Management**: Improved cleanup and memory management
- **Configuration Constants**: Added configurable parameters for stack/heap sizes
- **Better Diagnostics**: Enhanced logging and status messages

### Build System
- **Improved CMakeLists.txt**: Better library detection and error reporting
- **Build Scripts**: Added convenient `build.sh` and `run.sh` scripts
- **Cross-platform Support**: Better handling of library paths and linking

### Documentation
- **Organized Output**: Added section headers for different test categories
- **Comprehensive README**: Detailed documentation with examples and troubleshooting
- **Code Comments**: Better inline documentation and explanations

### Testing Coverage
- **All Component Model Types**: Comprehensive testing of primitives, composites, and advanced types
- **Error Scenarios**: Testing of error conditions and edge cases
- **Performance Examples**: Demonstrations of efficient memory usage patterns

## Related Files

- **`main.cpp`**: Main sample application
- **`CMakeLists.txt`**: Build configuration
- **`../wasm/sample.wit`**: WebAssembly Interface Types definition
- **`../wasm/main.cpp`**: Guest-side implementation
- **`../../include/wamr.hpp`**: WAMR integration header
- **`../../include/cmcpp/`**: Component Model C++ library headers

## Further Reading

- [WebAssembly Component Model Specification](https://github.com/WebAssembly/component-model)
- [WAMR Documentation](https://github.com/bytecodealliance/wasm-micro-runtime)
- [Component Model C++ Library Documentation](../../README.md)
