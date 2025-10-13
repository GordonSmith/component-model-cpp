# WIT Test Stub Generation

This directory contains tools for generating C++ host function stubs from the WIT grammar test suite.

## Overview

The stub generation scripts process all `.wit` files in the grammar test suite (`ref/wit-bindgen/tests/codegen`) and generate corresponding C++ host function bindings using the `wit-codegen` tool.

This is useful for:
- **Testing code generation**: Verify `wit-codegen` works across all test cases
- **Reference implementations**: See how different WIT features map to C++ types
- **Bulk validation**: Ensure the entire toolchain (grammar → parser → codegen) works end-to-end
- **Development**: Quickly generate stubs to experiment with Component Model features

## Scripts

### Python Script (Recommended)
`generate_test_stubs.py` - Feature-rich Python implementation with better error handling and progress reporting.

**Features:**
- Progress indicator with file counter
- Colored output for success/failure/skipped
- Verbose mode to see generated files
- Filter option to process specific files
- Detailed error messages
- Cross-platform compatible

**Usage:**
```bash
# Basic usage - generate all stubs
./generate_test_stubs.py

# Verbose mode - see what files are generated
./generate_test_stubs.py -v

# Filter specific files - only process matching patterns
./generate_test_stubs.py -f streams
./generate_test_stubs.py -f "wasi-"

# Custom paths
./generate_test_stubs.py \
  --test-dir ../ref/wit-bindgen/tests/codegen \
  --output-dir my_output \
  --codegen ../build/tools/wit-codegen/wit-codegen

# Show help
./generate_test_stubs.py --help
```

### Bash Script
`generate_test_stubs.sh` - Lightweight shell script for Unix-like systems.

**Features:**
- Fast and simple
- No dependencies beyond bash and standard tools
- Colored output
- Error tracking

**Usage:**
```bash
# Basic usage
./generate_test_stubs.sh

# With custom environment variables
WIT_TEST_DIR=../ref/wit-bindgen/tests/codegen \
OUTPUT_DIR=generated_stubs \
CODEGEN_TOOL=../build/tools/wit-codegen/wit-codegen \
  ./generate_test_stubs.sh
```

## Prerequisites

1. **Build wit-codegen tool:**
   ```bash
   cmake -B build -DBUILD_GRAMMAR=ON
   cmake --build build --target wit-codegen
   ```

2. **Initialize submodules** (if not already done):
   ```bash
   git submodule update --init --recursive
   ```

## Generated Output

For each `.wit` file in the test suite, up to three files are generated:

### 1. Header file (`<name>.hpp`)
Contains:
- **Host namespace**: Functions the host must implement (guest imports)
- **Guest namespace**: Type signatures for calling guest-exported functions
- Type definitions (enums, variants, records, flags)
- Namespace organization matching the WIT interface structure

**Example:**
```cpp
#pragma once
#include <cmcpp.hpp>

// Host implements these (guest imports them)
namespace host {
namespace logging {
    void log(cmcpp::string_t message);
    uint32_t get_level();
}
}

// Guest exports these (host calls them)
namespace guest {
namespace logging {
    // Type signatures for guest_function<> wrapper
    using log_t = void(cmcpp::string_t);
    using get_level_t = uint32_t();
}
}
```

### 2. WAMR Host Implementation (`<name>_wamr.hpp`)
Contains:
- WAMR-specific host function wrappers using `host_function<>`
- Automatic marshaling between WebAssembly and C++ types
- Ready to register with WAMR runtime

**Example:**
```cpp
#pragma once
#include <wamr.hpp>
#include "logging.hpp"

namespace wamr {
namespace logging {
    // WAMR wrapper with automatic type marshaling
    inline wasm_val_t log_host(
        wasm_exec_env_t exec_env,
        cmcpp::WamrStringView message) {
        host::logging::log(message);
        return {};
    }
}
}
```

### 3. WAMR Bindings (`<name>_wamr.cpp`)
Contains:
- WAMR NativeSymbol registration array
- Signature strings for WebAssembly function signatures
- Module registration code

**Example:**
```cpp
#include "logging_wamr.hpp"

namespace wamr {
namespace logging {

static NativeSymbol native_symbols[] = {
    {"log", (void*)&log_host, "(*)", nullptr},
    {"get-level", (void*)&get_level_host, "()i", nullptr},
};

void register_natives(wasm_module_t module) {
    wasm_runtime_register_natives(
        "logging",
        native_symbols,
        sizeof(native_symbols) / sizeof(NativeSymbol)
    );
}

} // namespace logging
} // namespace wamr
```

## Output Structure

The script preserves the directory structure of the test suite:

```
generated_stubs/
├── floats.hpp
├── floats_wamr.hpp
├── floats_wamr.cpp
├── streams.hpp
├── streams_wamr.hpp
├── streams_wamr.cpp
├── issue569/
│   └── wit/
│       └── deps/
│           └── io/
│               ├── streams.hpp
│               ├── streams_wamr.hpp
│               └── streams_wamr.cpp
└── wasi-io/
    └── wit/
        ├── streams.hpp
        ├── streams_wamr.hpp
        └── streams_wamr.cpp
```

**Note:** The directory structure from the test suite is fully preserved, so files in subdirectories will maintain their relative paths.

## Success Rates

The scripts report three categories:

1. **Successful**: Files where stubs were generated successfully
2. **Skipped**: Files that parsed but produced no output (e.g., world-only definitions)
3. **Failed**: Files that couldn't be processed due to errors

Typical success rate: 80-90% of test files (some are intentionally minimal or test error conditions)

## Using Generated Stubs

### As Reference
Browse the generated stubs to understand:
- How WIT types map to C++ types
- How to structure host function implementations
- How to integrate with WAMR runtime

### For Testing
Compile and test generated stubs:
```bash
# Compile a single stub
g++ -std=c++20 -I ../include \
  generated_stubs/floats.cpp \
  -c -o floats.o

# Or use in your CMake project
add_executable(my_test generated_stubs/floats.cpp)
target_link_libraries(my_test PRIVATE cmcpp)
```

### For Development
Copy and modify stubs for your own projects:
```bash
# Copy a stub to your project
cp generated_stubs/streams.hpp my_project/
cp generated_stubs/streams.cpp my_project/

# Implement the TODOs
vim my_project/streams.cpp
```

## Troubleshooting

### wit-codegen not found
```
Error: wit-codegen not found at ../build/tools/wit-codegen/wit-codegen
```
**Solution:** Build the code generator:
```bash
cmake --build build --target wit-codegen
```

### Test directory not found
```
Error: Test directory not found at ../ref/wit-bindgen/tests/codegen
```
**Solution:** Initialize git submodules:
```bash
git submodule update --init --recursive
```

### No output files generated
Some WIT files (especially world-only definitions) may not generate stubs because they don't contain functions to implement. This is expected and counted as "skipped".

### Generation failures
Check verbose output to see specific errors:
```bash
./generate_test_stubs.py -v
```

Common causes:
- Grammar parsing errors (should be rare after grammar testing)
- Unsupported WIT features (report as issue)
- Invalid WIT syntax in test files

## Integration with CI/CD

Add stub generation to your CI pipeline:

```yaml
# GitHub Actions example
- name: Generate and Validate Stubs
  run: |
    # Build code generator
    cmake --build build --target wit-codegen
    
    # Generate stubs
    cd test
    python3 generate_test_stubs.py -v
    
    # Try compiling a few
    cd generated_stubs
    for stub in *.cpp; do
      g++ -std=c++20 -I ../../include -c "$stub" || exit 1
    done
```

## See Also

- [TESTING_GRAMMAR.md](TESTING_GRAMMAR.md) - Grammar testing documentation
- [../tools/wit-codegen/README.md](../tools/wit-codegen/README.md) - Code generator documentation
- [../grammar/README.md](../grammar/README.md) - Grammar documentation
- [README.md](README.md) - Test suite overview
