# WIT Code Generator - ANTLR Grammar Integration

## Overview
The `wit-codegen` tool now uses the ANTLR4 grammar parser to accurately parse WIT (WebAssembly Interface Types) files and generate C++ host function bindings using the cmcpp library.

## Architecture

### Components
1. **ANTLR Grammar Parser** (`grammar/Wit.g4`)
   - Full WIT specification grammar
   - Generates C++ lexer/parser code via `generate-grammar` CMake target

2. **WitInterfaceVisitor** (custom visitor)
   - Extends `WitBaseVisitor` from ANTLR
   - Walks the parse tree to extract interfaces and functions
   - Handles package declarations, interface items, and function items

3. **TypeMapper**
   - Maps WIT types to cmcpp types
   - Supports: primitives, lists, options, results, tuples

4. **CodeGenerator**
   - Generates three files:
     - `.hpp` - Function declarations with cmcpp types
     - `.cpp` - Implementation stubs
     - `_bindings.cpp` - WAMR NativeSymbol registration

## Type Mappings

| WIT Type | cmcpp Type | Notes |
|----------|------------|-------|
| `bool` | `cmcpp::bool_t` | Alias for `bool` |
| `u8` | `uint8_t` | Standard C++ type |
| `u16` | `uint16_t` | Standard C++ type |
| `u32` | `uint32_t` | Standard C++ type |
| `u64` | `uint64_t` | Standard C++ type |
| `s8` | `int8_t` | Standard C++ type |
| `s16` | `int16_t` | Standard C++ type |
| `s32` | `int32_t` | Standard C++ type |
| `s64` | `int64_t` | Standard C++ type |
| `f32` | `cmcpp::float32_t` | Alias for `float` |
| `f64` | `cmcpp::float64_t` | Alias for `double` |
| `char` | `cmcpp::char_t` | Alias for `char32_t` |
| `string` | `cmcpp::string_t` | Alias for `std::string` (UTF-8) |
| `list<T>` | `cmcpp::list_t<T>` | Alias for `std::vector<T>` |
| `tuple<T1, T2, ...>` | `cmcpp::tuple_t<T1, T2, ...>` | Alias for `std::tuple<T1, T2, ...>` |
| `option<T>` | `cmcpp::option_t<T>` | Alias for `std::optional<T>` |
| `result<T, E>` | `cmcpp::result_t<T, E>` | Alias for `cmcpp::variant_t<T, E>` |
| `variant<T1, T2, ...>` | `cmcpp::variant_t<T1, T2, ...>` | Alias for `std::variant<T1, T2, ...>` |
| `enum { ... }` | `cmcpp::enum_t` | Alias for `uint32_t` |
| `flags { ... }` | `cmcpp::flags_t<...>` | Uses `std::bitset` internally |
| `record { ... }` | `cmcpp::record_t<Struct>` | Wraps aggregate struct types |

## Usage

```bash
# Build the tool
cmake --build build --target wit-codegen

# Generate C++ bindings from WIT file
./build/tools/wit-parser/wit-codegen <wit-file> <output-prefix>

# Example
./build/tools/wit-parser/wit-codegen test-example.wit host_functions
```

This generates:
- `host_functions.hpp` - Header with function declarations
- `host_functions.cpp` - Implementation stubs
- `host_functions_bindings.cpp` - WAMR runtime bindings

## Example

### Input WIT file (`test-example.wit`):
```wit
package example:hello@1.0.0;

interface logging {
    log: func(message: string);
    get-level: func() -> u32;
    process-items: func(items: list<string>) -> list<u32>;
}
```

### Generated Header (`host_functions.hpp`):
```cpp
#pragma once
#include <cmcpp.hpp>

// Generated host function declarations from WIT
// Functions in 'host' namespace are imported by the guest (host implements them)

namespace host {

namespace logging {
    // Host function declaration
    void log(cmcpp::string_t message);
    
    // Guest function signature typedef for use with guest_function<log_t>()
    using log_t = void(cmcpp::string_t);

    // Host function declaration
    uint32_t get_level();
    
    // Guest function signature typedef for use with guest_function<get_level_t>()
    using get_level_t = uint32_t();

    // Host function declaration
    cmcpp::list_t<uint32_t> process_items(cmcpp::list_t<cmcpp::string_t> items);
    
    // Guest function signature typedef for use with guest_function<process_items_t>()
    using process_items_t = cmcpp::list_t<uint32_t>(cmcpp::list_t<cmcpp::string_t>);
}

} // namespace host
```

The generated header includes:
1. **Namespace organization** - All imported interfaces are wrapped in the `host` namespace to prevent name clashes with guest exports
2. **Host function declarations** - Functions to be implemented on the host side
3. **Guest function signature typedefs** - Type aliases ending in `_t` that can be used with `guest_function<T>()` template to call guest-exported functions

Example usage of guest function typedef:
```cpp
#include "host_functions.hpp"
#include <wamr.hpp>

// In your host code, call a guest function:
auto guest_log = guest_function<imports::logging::log_t>(
    module_inst, exec_env, liftLowerContext, 
    "example:hello/logging#log"
);
guest_log("Hello from guest!");
```

### Generated Implementation (`host_functions.cpp`):
```cpp
#include "host_functions.hpp"

namespace imports {

namespace logging {
    void log(cmcpp::string_t message) {
        // TODO: Implement log
    }
}

} // namespace imports
```

### Generated Bindings (`host_functions_bindings.cpp`):
```cpp
#include <wamr.hpp>
#include <vector>

extern "C" {
    extern void logging::log_wrapper();
}

std::vector<NativeSymbol> get_native_symbols() {
    return {
        {"logging#log", (void*)logging::log_wrapper, ""},
    };
}
```

## Grammar Visitor Implementation

The visitor follows the ANTLR parse tree structure:

```cpp
class WitInterfaceVisitor : public WitBaseVisitor {
    // visitPackageDecl - Extract package information
    // visitInterfaceItem - Start new interface context
    // visitFuncItem - Extract function name, parameters, and results
};
```

Key grammar rules used:
- `funcItem: id ':' funcType ';'`
- `funcType: 'func' paramList resultList`
- `paramList: '(' namedTypeList ')'`
- `resultList: /* epsilon */ | '->' ty`
- `namedType: id ':' ty`

## Integration with Component Model C++

The generated code uses the cmcpp header-only library which implements the WebAssembly Component Model canonical ABI. This ensures type-safe marshaling between C++ and WebAssembly components.

## Using Generated Guest Function Typedefs

The generated header file includes typedef aliases for each function that can be used with the `guest_function<T>()` template from `wamr.hpp`. This provides type-safe calling of guest-exported functions.

### Example: Calling Guest Functions

```cpp
#include "host_functions.hpp"
#include <wamr.hpp>
#include <iostream>

int main() {
    // ... WAMR initialization code ...
    
    // Create a lift/lower context
    cmcpp::LiftLowerContext liftLowerContext(/* ... */);
    
    // Call a guest function using the generated typedef
    auto guest_log = guest_function<logging::log_t>(
        module_inst, exec_env, liftLowerContext,
        "example:hello/logging#log"
    );
    guest_log("Hello from host!");
    
    // Call a guest function that returns a value
    auto guest_get_level = guest_function<logging::get_level_t>(
        module_inst, exec_env, liftLowerContext,
        "example:hello/logging#get-level"
    );
    uint32_t level = guest_get_level();
    std::cout << "Guest log level: " << level << std::endl;
    
    // Call a guest function with complex types
    auto guest_process = guest_function<logging::process_items_t>(
        module_inst, exec_env, liftLowerContext,
        "example:hello/logging#process-items"
    );
    cmcpp::list_t<cmcpp::string_t> input = {"foo", "bar", "baz"};
    auto result = guest_process(input);
    for (auto val : result) {
        std::cout << "Result: " << val << std::endl;
    }
    
    return 0;
}
```

### Benefits of Generated Typedefs

1. **Type Safety** - The compiler ensures you're using the correct parameter and return types
2. **Code Clarity** - The typedef name clearly indicates which guest function it corresponds to
3. **Autocomplete** - IDEs can provide better autocomplete suggestions
4. **Maintainability** - Changes to the WIT file automatically update the typedefs

## Build Dependencies

- ANTLR4 runtime (from vcpkg) - Linked directly by wit-codegen
- ANTLR4 code generator (antlr-4.13.2-complete.jar) - Used by generate-grammar CMake target
- cmcpp headers (Component Model C++ ABI)
- wamr.hpp (for WAMR runtime integration)

The `wit-codegen` tool includes the ANTLR-generated source files directly and links to the ANTLR4 runtime library. The grammar code is generated by the `generate-grammar` CMake target.
