# Generated WAMR Helper Functions

The WIT code generator produces three files to simplify WAMR host application development:

1. **`<name>.hpp`** - Component Model interface declarations (host and guest functions)
2. **`<name>_wamr.hpp`** - WAMR helper function declarations
3. **`<name>_wamr.cpp`** - WAMR symbol arrays and helper function implementations

Where `<name>` is the output prefix you provide to `wit-codegen`.

## Usage

Simply include the WAMR header to access all helper functions:

```cpp
#include "<name>_wamr.hpp"
```

This automatically includes:
- `<wamr.hpp>` - WAMR runtime API
- `<cmcpp.hpp>` - Component Model C++ types
- `"<name>.hpp"` - Your generated interface declarations

## Generated Helper Functions

### 1. Registration Helpers

#### `register_all_imports()`
Registers all imported interfaces at once. Returns the number of functions registered, or -1 on failure.

**Before:**
```cpp
for (const auto& reg : get_import_registrations()) {
    bool success = wasm_runtime_register_natives_raw(reg.module_name, reg.symbols, reg.count);
    if (!success) {
        std::cerr << "Failed to register natives for: " << reg.module_name << std::endl;
        return 1;
    }
}
```

**After:**
```cpp
int count = register_all_imports();
if (count < 0) {
    std::cerr << "Failed to register native imports" << std::endl;
    return 1;
}
std::cout << "Registered " << count << " host functions" << std::endl;
```

#### `unregister_all_imports()`
Unregisters all imported interfaces at cleanup time.

**Before:**
```cpp
for (const auto& reg : get_import_registrations()) {
    wasm_runtime_unregister_natives(reg.module_name, reg.symbols);
}
```

**After:**
```cpp
unregister_all_imports();
```

### 2. WASM Runtime Constants

The generated WAMR bindings provide default configuration constants:

```cpp
wasm_utils::DEFAULT_STACK_SIZE  // 8192
wasm_utils::DEFAULT_HEAP_SIZE   // 8192
```

### 3. Context Creation Functions (from `<wamr.hpp>`)

The following helper functions are available in the `cmcpp` namespace from `<wamr.hpp>` (not generated, but part of the cmcpp library):

#### `cmcpp::create_guest_realloc()`
Creates a `GuestRealloc` function from WAMR execution environment.

**Usage:**
```cpp
wasm_function_inst_t cabi_realloc = wasm_runtime_lookup_function(module_inst, "cabi_realloc");
cmcpp::GuestRealloc realloc = cmcpp::create_guest_realloc(exec_env, cabi_realloc);
```

#### `cmcpp::create_lift_lower_context()`
Creates a complete `LiftLowerContext` ready for use with `guest_function<>()`.

**Before:**
```cpp
wasm_memory_inst_t memory = wasm_runtime_lookup_memory(module_inst, "memory");
if (!memory) {
    std::cerr << "Failed to lookup memory instance" << std::endl;
    return 1;
}

uint8_t* mem_start_addr = (uint8_t*)wasm_memory_get_base_address(memory);
uint8_t* mem_end_addr = NULL;
wasm_runtime_get_native_addr_range(module_inst, mem_start_addr, NULL, &mem_end_addr);

GuestRealloc realloc = [exec_env, cabi_realloc](int original_ptr, int original_size, int alignment, int new_size) -> int {
    uint32_t argv[4];
    argv[0] = original_ptr;
    argv[1] = original_size;
    argv[2] = alignment;
    argv[3] = new_size;
    wasm_runtime_call_wasm(exec_env, cabi_realloc, 4, argv);
    return argv[0];
};

LiftLowerOptions opts(Encoding::Utf8, std::span<uint8_t>(mem_start_addr, mem_end_addr - mem_start_addr), realloc);
LiftLowerContext ctx(trap, convert, opts);
```

**After:**
```cpp
wasm_function_inst_t cabi_realloc = wasm_runtime_lookup_function(module_inst, "cabi_realloc");
LiftLowerContext ctx = cmcpp::create_lift_lower_context(module_inst, exec_env, cabi_realloc);
```

**Parameters:**
- `module_inst`: WAMR module instance
- `exec_env`: WAMR execution environment
- `cabi_realloc`: The `cabi_realloc` function from the WASM module
- `encoding`: String encoding (default: `Encoding::Utf8`)

**Throws:** `std::runtime_error` if memory lookup fails

## Example: Simplified main() Function

Here's how a typical `main()` function looks using the generated helpers:

```cpp
#include <wamr.hpp>
#include "test_wamr.hpp"  // Includes test.hpp automatically
#include <iostream>
#include <fstream>
#include <filesystem>

int main(int argc, char** argv) {
    // Initialize WAMR
    wasm_runtime_init();
    
    // Read WASM file (you need to implement this based on your application)
    std::ifstream file("path/to/your.wasm", std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Failed to open WASM file" << std::endl;
        return 1;
    }
    
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) {
        std::cerr << "Failed to read WASM file" << std::endl;
        return 1;
    }
    
    // Register all host functions
    int count = register_all_imports();
    if (count < 0) {
        std::cerr << "Failed to register imports" << std::endl;
        return 1;
    }
    std::cout << "Registered " << count << " host functions" << std::endl;
    
    // Load and instantiate module
    char error_buf[128];
    wasm_module_t module = wasm_runtime_load(
        (uint8_t*)buffer.data(), size, error_buf, sizeof(error_buf));
    if (!module) {
        std::cerr << "Failed to load module: " << error_buf << std::endl;
        return 1;
    }
    
    wasm_module_inst_t module_inst = wasm_runtime_instantiate(
        module, wasm_utils::DEFAULT_STACK_SIZE, wasm_utils::DEFAULT_HEAP_SIZE, 
        error_buf, sizeof(error_buf));
    if (!module_inst) {
        std::cerr << "Failed to instantiate: " << error_buf << std::endl;
        wasm_runtime_unload(module);
        return 1;
    }
    
    // Create execution environment and context
    wasm_exec_env_t exec_env = wasm_runtime_create_exec_env(
        module_inst, wasm_utils::DEFAULT_STACK_SIZE);
    
    wasm_function_inst_t cabi_realloc = wasm_runtime_lookup_function(
        module_inst, "cabi_realloc");
    cmcpp::LiftLowerContext ctx = cmcpp::create_lift_lower_context(
        module_inst, exec_env, cabi_realloc);
    
    // Call guest functions using the generated typedefs
    auto my_func = guest_function<guest::my_func_t>(
        module_inst, exec_env, ctx, "example:sample/namespace#my-func");
    auto result = my_func(/* args */);
    
    // Cleanup
    wasm_runtime_destroy_exec_env(exec_env);
    unregister_all_imports();
    wasm_runtime_deinstantiate(module_inst);
    wasm_runtime_unload(module);
    wasm_runtime_destroy();
    
    return 0;
}
```

## Benefits

1. **Reduced Boilerplate**: Registration and cleanup happen in two function calls rather than manually iterating arrays
2. **Automatic Constants**: Standard stack and heap sizes are provided as named constants
3. **Less Error-Prone**: No manual copying of function names or managing registration state
4. **Type Safety**: Generated function typedefs in the `guest::` namespace provide compile-time type checking
5. **Separation of Concerns**: Host interface definitions in `host::` namespace, guest signatures in `guest::` namespace
6. **Easier Maintenance**: Changes to WIT automatically regenerate all registration code

## Migration Guide

If you have existing code without generated helpers:

1. **Replace manual registration loops** with a single call to `register_all_imports()`
2. **Replace manual unregistration** with `unregister_all_imports()`
3. **Use generated constants** `wasm_utils::DEFAULT_STACK_SIZE` and `wasm_utils::DEFAULT_HEAP_SIZE` for consistency
4. **Update namespace references**: Host functions are in `host::` namespace, guest type signatures are in `guest::` namespace
5. **Use helper functions from wamr.hpp**: `cmcpp::create_guest_realloc()` and `cmcpp::create_lift_lower_context()` for context setup

### Example Transformation

**Before (manual registration):**
```cpp
// Manual array iteration
for (int i = 0; test_wamr_imports[i].symbol; i++) {
    if (!wasm_runtime_register_natives(
            "example:test/namespace", 
            &test_wamr_imports[i], 1)) {
        // handle error
    }
}
```

**After (generated helpers):**
```cpp
// Single call
int count = register_all_imports();
if (count < 0) {
    // handle error
}
```

## Note on Root-Level Functions

Functions defined at the world level (not inside an interface) are included in the generated registration code if they have import statements. They appear in the `host::` namespace just like interface functions:

```cpp
// In your WIT file:
// package example:test;
// 
// world test-world {
//     import root-func: func(x: u32) -> u32;
//     
//     interface my-interface {
//         import interface-func: func(s: string);
//     }
// }

// Generated in test.hpp:
namespace host {
    // Root-level function
    cmcpp::u32_t root_func(cmcpp::u32_t x);
    
    // Interface function  
    void interface_func(cmcpp::string_t s);
}
```

Both types of imports are automatically registered by `register_all_imports()`. The namespace structure in the generated code is `host::` for all imports and `guest::` for all exports, regardless of whether they are at root level or in an interface.
