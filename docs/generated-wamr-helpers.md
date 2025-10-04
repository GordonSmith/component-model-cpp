# Generated WAMR Helper Functions

The WIT code generator now produces three files to simplify WAMR host application development:

1. **`sample.hpp`** - Component Model interface declarations (imports and exports)
2. **`sample_wamr.hpp`** - WAMR helper function declarations
3. **`sample_wamr.cpp`** - WAMR symbol arrays and helper function implementations

## Usage

Simply include the WAMR header to access all helper functions:

```cpp
#include "sample_wamr.hpp"
```

This automatically includes:
- `<wamr.hpp>` - WAMR runtime API
- `<cmcpp.hpp>` - Component Model C++ types
- `"sample.hpp"` - Your generated interface declarations

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

### 2. WASM File Utilities (`wasm_utils` namespace)

#### Constants
```cpp
wasm_utils::DEFAULT_STACK_SIZE  // 8192
wasm_utils::DEFAULT_HEAP_SIZE   // 8192
```

#### `wasm_utils::resolve_wasm_path()`
Searches up the directory tree to find the WASM file.

**Usage:**
```cpp
std::filesystem::path wasm_path = wasm_utils::resolve_wasm_path(exe_dir);
if (wasm_path.empty()) {
    std::cerr << "Unable to locate sample.wasm" << std::endl;
    return 1;
}
```

**Parameters:**
- `start_dir`: Directory to start searching from
- `relative_path`: Path to search for (default: `bin/sample.wasm`)
- `max_ascent`: Maximum directory levels to search up (default: 6)

#### `wasm_utils::read_wasm_binary_to_buffer()`
Reads a WASM binary file into memory.

**Usage:**
```cpp
uint32_t size;
char* buffer = wasm_utils::read_wasm_binary_to_buffer(wasm_path, &size);
if (!buffer) {
    std::cerr << "Failed to read WASM file" << std::endl;
    return 1;
}
// ... use buffer ...
delete[] buffer;  // Don't forget to free!
```

#### `wasm_utils::create_guest_realloc()`
Creates a `GuestRealloc` function from WAMR execution environment.

**Usage:**
```cpp
wasm_function_inst_t cabi_realloc = wasm_runtime_lookup_function(module_inst, "cabi_realloc");
cmcpp::GuestRealloc realloc = wasm_utils::create_guest_realloc(exec_env, cabi_realloc);
```

#### `wasm_utils::create_lift_lower_context()`
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
LiftLowerContext ctx = wasm_utils::create_lift_lower_context(module_inst, exec_env, cabi_realloc);
```

**Parameters:**
- `module_inst`: WAMR module instance
- `exec_env`: WAMR execution environment
- `cabi_realloc`: The `cabi_realloc` function from the WASM module
- `encoding`: String encoding (default: `Encoding::Utf8`)

**Throws:** `std::runtime_error` if memory lookup fails

## Example: Simplified main() Function

Here's how a typical `main()` function can be simplified using the generated helpers:

```cpp
#include "wamr.hpp"
#include "sample_wamr.hpp"  // Includes sample.hpp automatically

int main(int argc, char** argv) {
    // Initialize WAMR
    wasm_runtime_init();
    
    // Find and load WASM file
    std::filesystem::path exe_dir = std::filesystem::absolute(argv[0]).parent_path();
    std::filesystem::path wasm_path = wasm_utils::resolve_wasm_path(exe_dir);
    if (wasm_path.empty()) {
        std::cerr << "Could not find sample.wasm" << std::endl;
        return 1;
    }
    
    uint32_t size;
    char* buffer = wasm_utils::read_wasm_binary_to_buffer(wasm_path, &size);
    if (!buffer) {
        std::cerr << "Failed to read WASM file" << std::endl;
        return 1;
    }
    
    // Register all host functions
    int count = register_all_imports();
    if (count < 0) {
        std::cerr << "Failed to register imports" << std::endl;
        delete[] buffer;
        return 1;
    }
    
    // Load and instantiate module
    char error_buf[128];
    wasm_module_t module = wasm_runtime_load((uint8_t*)buffer, size, error_buf, sizeof(error_buf));
    if (!module) {
        std::cerr << "Failed to load module: " << error_buf << std::endl;
        delete[] buffer;
        return 1;
    }
    
    wasm_module_inst_t module_inst = wasm_runtime_instantiate(
        module, wasm_utils::DEFAULT_STACK_SIZE, wasm_utils::DEFAULT_HEAP_SIZE, 
        error_buf, sizeof(error_buf));
    if (!module_inst) {
        std::cerr << "Failed to instantiate: " << error_buf << std::endl;
        wasm_runtime_unload(module);
        delete[] buffer;
        return 1;
    }
    
    // Create execution environment and context
    wasm_exec_env_t exec_env = wasm_runtime_create_exec_env(
        module_inst, wasm_utils::DEFAULT_STACK_SIZE);
    
    wasm_function_inst_t cabi_realloc = wasm_runtime_lookup_function(module_inst, "cabi_realloc");
    LiftLowerContext ctx = wasm_utils::create_lift_lower_context(module_inst, exec_env, cabi_realloc);
    
    // Call guest functions
    auto my_func = guest_function<exports::my_namespace::my_func_t>(
        module_inst, exec_env, ctx, "example:sample/my-namespace#my-func");
    auto result = my_func(/* args */);
    
    // Cleanup
    delete[] buffer;
    wasm_runtime_destroy_exec_env(exec_env);
    unregister_all_imports();
    wasm_runtime_deinstantiate(module_inst);
    wasm_runtime_unload(module);
    wasm_runtime_destroy();
    
    return 0;
}
```

## Benefits

1. **Less Boilerplate**: ~80 lines of boilerplate code reduced to ~10-15 lines
2. **Error Handling**: Built-in error checking with clear failure modes
3. **Consistency**: Same initialization code across all WAMR host applications
4. **Maintainability**: Updates to WAMR integration only need to change the code generator
5. **Type Safety**: Proper C++ types and RAII principles where possible

## Migration Guide

To update existing code to use the new helpers:

1. **Registration**: Replace manual loops with `register_all_imports()` and `unregister_all_imports()`
2. **File Loading**: Replace custom file reading with `wasm_utils::read_wasm_binary_to_buffer()`
3. **Path Resolution**: Use `wasm_utils::resolve_wasm_path()` instead of custom path logic
4. **Context Creation**: Replace ~30 lines of context setup with `wasm_utils::create_lift_lower_context()`
5. **Constants**: Use `wasm_utils::DEFAULT_STACK_SIZE` and `wasm_utils::DEFAULT_HEAP_SIZE`

## Note on Root-Level Functions

Root-level functions (those not in an interface) must still be manually registered, as they are application-specific:

```cpp
// User-defined root functions
void_t my_root_func() { /* implementation */ }

NativeSymbol root_symbols[] = {
    host_function("my-root-func", my_root_func),
};

// Register before loading module
wasm_runtime_register_natives_raw("$root", root_symbols, sizeof(root_symbols) / sizeof(NativeSymbol));

// ... use module ...

// Unregister during cleanup
wasm_runtime_unregister_natives("$root", root_symbols);
```
