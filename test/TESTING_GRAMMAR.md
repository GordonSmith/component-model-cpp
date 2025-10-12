# WIT Grammar Testing Guide

## Overview

This directory contains a comprehensive test suite for the WIT (WebAssembly Interface Types) grammar implementation using ANTLR4. The tests validate the grammar against the official test suite from the [wit-bindgen](https://github.com/bytecodealliance/wit-bindgen) project.

## Quick Start

```bash
# 1. Configure CMake with grammar support
cmake -B build -DBUILD_GRAMMAR=ON

# 2. Build the grammar and test executable
cmake --build build --target test-wit-grammar

# 3. Run the tests
cd build && ctest -R wit-grammar-test --verbose
```

## What Gets Tested

The test suite validates the grammar against **95+ official WIT test files** including:

### Basic Types
- ✅ Integers (u8, u16, u32, u64, s8, s16, s32, s64)
- ✅ Floats (f32, f64)
- ✅ Strings and characters
- ✅ Booleans

### Complex Types
- ✅ Records (structs with named fields)
- ✅ Variants (tagged unions)
- ✅ Enums (variants without payloads)
- ✅ Flags (bitsets)
- ✅ Tuples
- ✅ Lists (including fixed-length lists)
- ✅ Options and Results

### Async Features (Recently Added)
- ✅ Async functions (`async func`)
- ✅ Futures (`future<T>` and `future`)
- ✅ Streams (`stream<T>` and `stream`)

### Resources
- ✅ Resource declarations
- ✅ Resource methods
- ✅ Static resource functions
- ✅ Constructors (infallible and fallible)
- ✅ Owned handles (`resource-name`)
- ✅ Borrowed handles (`borrow<resource-name>`)

### Package & Interface Features
- ✅ Package declarations with versions
- ✅ Nested namespaces and packages
- ✅ Interface definitions
- ✅ World definitions
- ✅ Import and export statements
- ✅ Use statements (local and cross-package)
- ✅ Include statements with renaming

### Feature Gates
- ✅ `@unstable(feature = name)` gates
- ✅ `@since(version = x.y.z)` gates
- ✅ `@deprecated(version = x.y.z)` gates

### Real-World Examples
- ✅ WASI specifications (wasi-clocks, wasi-filesystem, wasi-http, wasi-io)
- ✅ Error contexts
- ✅ Complex nested types
- ✅ Multiple interfaces and worlds per file

## Test Files Location

The test suite uses files from:
```
ref/wit-bindgen/tests/codegen/
```

This directory is included as a git submodule and contains the canonical test suite maintained by the WebAssembly community.

## Running Tests

### Method 1: Using CTest (Recommended)

**Linux/macOS:**
```bash
# From the build directory
cd build
ctest -R wit-grammar-test

# With verbose output
ctest -R wit-grammar-test --verbose

# Run all tests including grammar
ctest --output-on-failure
```

**Windows (Visual Studio):**
```bash
# From the build directory
cd build

# Specify configuration (Debug or Release)
ctest -C Release -R wit-grammar-test

# With verbose output
ctest -C Release -R wit-grammar-test --verbose

# Run all tests
ctest -C Release --output-on-failure
```

### Method 2: Direct Execution

**Linux/macOS:**
```bash
# From project root
./build/test/test-wit-grammar

# With verbose output (shows each file being parsed)
./build/test/test-wit-grammar --verbose

# Custom test directory
./build/test/test-wit-grammar --directory /path/to/wit/files
```

**Windows:**
```cmd
REM From project root (adjust Debug/Release as needed)
.\build\test\Release\test-wit-grammar.exe

REM With verbose output
.\build\test\Release\test-wit-grammar.exe --verbose

REM Custom test directory
.\build\test\Release\test-wit-grammar.exe --directory C:\path\to\wit\files
```

## Test Output

### Success Output
```
WIT Grammar Test Suite
======================
Test directory: ../ref/wit-bindgen/tests/codegen

Found 95 WIT files

======================
Test Results:
  Total files:  95
  Successful:   95
  Failed:       0

✓ All tests passed!
```

### Failure Output
When parsing fails, detailed error messages are provided:
```
Failed to parse: example.wit
Errors in example.wit:
  line 5:10 mismatched input 'invalid' expecting {'u8', 'u16', ...}
  
======================
Test Results:
  Total files:  95
  Successful:   94
  Failed:       1

Failed files:
  - example.wit
```

## Troubleshooting

### Java Not Found
```
Error: Java runtime not found. ANTLR4 requires Java to generate code.
```
**Solution:** Install Java Runtime Environment (JRE) 8 or later.

### ANTLR Jar Not Found
The CMake configuration automatically downloads ANTLR. If this fails:
```bash
# Manually download
wget https://www.antlr.org/download/antlr-4.13.2-complete.jar -O antlr-4.13.2-complete.jar
```

### Submodule Not Initialized
```
Error: wit-bindgen test files not found
```
**Solution:**
```bash
git submodule update --init ref/wit-bindgen
```

### Build Errors
If the grammar library fails to build:
```bash
# Clean build and regenerate
rm -rf build
cmake -B build -DBUILD_GRAMMAR=ON
cmake --build build --target generate-grammar
cmake --build build --target test-wit-grammar
```

## Continuous Integration

The grammar tests can be integrated into CI/CD pipelines:

```yaml
# Example GitHub Actions workflow
- name: Test WIT Grammar
  run: |
    cmake -B build -DBUILD_GRAMMAR=ON
    cmake --build build --target test-wit-grammar
    cd build && ctest -R wit-grammar-test --output-on-failure
```

## Adding New Test Files

To test against additional WIT files:

1. Add `.wit` files to any directory
2. Run with custom directory:
   ```bash
   ./build/grammar/test-wit-grammar --directory /path/to/your/wit/files
   ```

## Generating C++ Stubs from Test Files

The test suite includes tools to generate C++ host function stubs from all WIT test files. This is useful for:
- Testing the `wit-codegen` code generator
- Creating reference implementations
- Validating end-to-end WIT parsing and code generation

### Quick Start

**Python script (recommended):**
```bash
# Generate stubs for all test files
./test/generate_test_stubs.py

# Verbose output
./test/generate_test_stubs.py -v

# Filter specific files
./test/generate_test_stubs.py -f "streams"

# Custom paths
./test/generate_test_stubs.py \
  --test-dir ref/wit-bindgen/tests/codegen \
  --output-dir my_stubs \
  --codegen build/tools/wit-codegen/wit-codegen
```

**Bash script:**
```bash
# Generate stubs for all test files
./test/generate_test_stubs.sh

# With custom environment variables
WIT_TEST_DIR=ref/wit-bindgen/tests/codegen \
OUTPUT_DIR=generated_stubs \
CODEGEN_TOOL=build/tools/wit-codegen/wit-codegen \
  ./test/generate_test_stubs.sh
```

### Generated Files

For each `.wit` file, the generator creates:
- `<name>.hpp` - Host function declarations with cmcpp types
- `<name>.cpp` - Implementation stubs (empty functions to fill in)
- `<name>_bindings.cpp` - WAMR NativeSymbol registration

The directory structure of the test suite is preserved in the output.

### Example Output

For a test file `ref/wit-bindgen/tests/codegen/streams.wit`:
```
generated_stubs/
└── streams.hpp
└── streams.cpp
└── streams_bindings.cpp
```

### Prerequisites

Before generating stubs, ensure `wit-codegen` is built:
```bash
cmake -B build -DBUILD_GRAMMAR=ON
cmake --build build --target wit-codegen
```

### Script Options

**Python script options:**
- `-d, --test-dir PATH` - Directory containing WIT test files (default: `../ref/wit-bindgen/tests/codegen`)
- `-o, --output-dir PATH` - Output directory for stubs (default: `generated_stubs`)
- `-c, --codegen PATH` - Path to wit-codegen tool (default: `../build/tools/wit-codegen/wit-codegen`)
- `-v, --verbose` - Print detailed output including generated files
- `-f, --filter PATTERN` - Only process files matching the pattern

**Bash script environment variables:**
- `WIT_TEST_DIR` - Test directory path
- `OUTPUT_DIR` - Output directory path
- `CODEGEN_TOOL` - Code generator tool path

### Using Generated Stubs

The generated stubs can be used as:
1. **Reference implementations** - See how different WIT features map to C++
2. **Integration tests** - Verify the full toolchain works end-to-end
3. **Starting point** - Copy and modify for your own projects

To compile generated stubs:
```bash
# Link against cmcpp and your WebAssembly runtime
g++ -std=c++20 -I include generated_stubs/streams.cpp -o my_host
```

## Grammar Coverage

The test suite ensures complete coverage of the WIT specification:
- ✅ All keywords (as, async, bool, borrow, char, constructor, enum, export, f32, f64, flags, from, func, future, import, include, interface, list, option, own, package, record, resource, result, s8-s64, static, stream, string, tuple, type, u8-u64, use, variant, with, world)
- ✅ All operators (=, ,, :, ;, (, ), {, }, <, >, *, ->, /, ., @)
- ✅ Package versions (SemVer 2.0.0)
- ✅ Comments (line and block)
- ✅ Nested structures

## Related Documentation

- [WIT Specification](../ref/component-model/design/mvp/WIT.md)
- [Grammar File](../grammar/Wit.g4)
- [Grammar README](../grammar/README.md)
- [Project README](../README.md)
- [Test README](README.md)
