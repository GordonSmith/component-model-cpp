# Component Model C++ Tests

This directory contains the test suite for the component-model-cpp library.

## Test Components

### 1. Component Model ABI Tests (`main.cpp`, `scratchpad.cpp`)

Core tests for the WebAssembly Component Model canonical ABI implementation:
- Type marshaling (lift/lower operations)
- String encoding conversions
- List handling
- Record and variant types
- Resource management
- Async primitives (futures, streams, tasks)
- Error contexts

**Running:**
```bash
cmake --build build
cd build && ctest -R component-model-test
# Or directly:
./build/test/component-model-test
```

### 2. WIT Grammar Tests (`test_grammar.cpp`)

Validates the ANTLR4-based WIT parser against official test files from the [wit-bindgen](https://github.com/bytecodealliance/wit-bindgen) project.

#### Requirements
- Java Runtime Environment (for ANTLR)
- CMake flag: `-DBUILD_GRAMMAR=ON`

#### Quick Start

**Linux/macOS:**
```bash
# 1. Configure CMake with grammar support
cmake -B build -DBUILD_GRAMMAR=ON

# 2. Build the grammar and test executable
cmake --build build --target test-wit-grammar

# 3. Run the tests
cd build && ctest -R wit-grammar-test --verbose
```

**Windows:**
```bash
# 1. Configure CMake with grammar support
cmake -B build -DBUILD_GRAMMAR=ON

# 2. Build the grammar and test executable (specify configuration)
cmake --build build --target test-wit-grammar --config Release

# 3. Run the tests (must specify -C configuration on Windows)
cd build && ctest -C Release -R wit-grammar-test --verbose
```

#### What Gets Tested

The test suite validates the grammar against **95+ official WIT test files** including:

**Basic Types**
- ✅ Integers (u8, u16, u32, u64, s8, s16, s32, s64)
- ✅ Floats (f32, f64)
- ✅ Strings and characters
- ✅ Booleans

**Complex Types**
- ✅ Records (structs with named fields)
- ✅ Variants (tagged unions)
- ✅ Enums (variants without payloads)
- ✅ Flags (bitsets)
- ✅ Tuples
- ✅ Lists (including fixed-length lists)
- ✅ Options and Results

**Async Features (Recently Added)**
- ✅ Async functions (`async func`)
- ✅ Futures (`future<T>` and `future`)
- ✅ Streams (`stream<T>` and `stream`)

**Resources**
- ✅ Resource declarations
- ✅ Resource methods
- ✅ Static resource functions
- ✅ Constructors (infallible and fallible)
- ✅ Owned handles (`resource-name`)
- ✅ Borrowed handles (`borrow<resource-name>`)

**Package & Interface Features**
- ✅ Package declarations with versions
- ✅ Nested namespaces and packages
- ✅ Interface definitions
- ✅ World definitions
- ✅ Import and export statements
- ✅ Use statements (local and cross-package)
- ✅ Include statements with renaming

**Feature Gates**
- ✅ `@unstable(feature = name)` gates
- ✅ `@since(version = x.y.z)` gates
- ✅ `@deprecated(version = x.y.z)` gates

**Real-World Examples**
- ✅ WASI specifications (wasi-clocks, wasi-filesystem, wasi-http, wasi-io)
- ✅ Error contexts
- ✅ Complex nested types
- ✅ Multiple interfaces and worlds per file

#### Test Files Location

The test suite uses files from:
```
ref/wit-bindgen/tests/codegen/
```

This directory is included as a git submodule and contains the canonical test suite maintained by the WebAssembly community.

#### Running Grammar Tests

**Method 1: Using CTest (Recommended)**

```bash
# From the build directory
cd build
ctest -R wit-grammar-test

# With verbose output
ctest -R wit-grammar-test --verbose

# Run all tests including grammar
ctest
```

**Method 2: Direct Execution**

```bash
# From project root
./build/test/test-wit-grammar

# With verbose output (shows each file being parsed)
./build/test/test-wit-grammar --verbose

# Custom test directory
./build/test/test-wit-grammar --directory /path/to/wit/files
```

**Method 3: Using VS Code**

Use the VS Code debugger with the provided launch configurations:
- **`(gdb) test-wit-grammar`** - Run grammar tests on Linux
- **`(gdb) test-wit-grammar (verbose)`** - Run with verbose output on Linux
- **`(win) test-wit-grammar`** - Run grammar tests on Windows
- **`(win) test-wit-grammar (verbose)`** - Run with verbose output on Windows

Press `F5` or use the Run and Debug panel (Ctrl+Shift+D) to select and run a configuration.

#### Test Output

**Success Output:**
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

**Failure Output:**
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

#### Troubleshooting Grammar Tests

**Java Not Found**
```
Error: Java runtime not found. ANTLR4 requires Java to generate code.
```
*Solution:* Install Java Runtime Environment (JRE) 8 or later.

**ANTLR Jar Not Found**

The CMake configuration automatically downloads ANTLR. If this fails:
```bash
# Manually download
wget https://www.antlr.org/download/antlr-4.13.2-complete.jar -O antlr-4.13.2-complete.jar
```

**Submodule Not Initialized**
```
Error: wit-bindgen test files not found
```
*Solution:*
```bash
git submodule update --init ref/wit-bindgen
```

**Build Errors**

If the grammar library fails to build:
```bash
# Clean build and regenerate
rm -rf build
cmake -B build -DBUILD_GRAMMAR=ON
cmake --build build --target generate-grammar
cmake --build build --target test-wit-grammar
```

#### Grammar Coverage

The test suite ensures complete coverage of the WIT specification:
- ✅ All keywords (as, async, bool, borrow, char, constructor, enum, export, f32, f64, flags, from, func, future, import, include, interface, list, option, own, package, record, resource, result, s8-s64, static, stream, string, tuple, type, u8-u64, use, variant, with, world)
- ✅ All operators (=, ,, :, ;, (, ), {, }, <, >, *, ->, /, ., @)
- ✅ Package versions (SemVer 2.0.0)
- ✅ Comments (line and block)
- ✅ Nested structures

## Running All Tests

```bash
# Build all tests
cmake --build build

# Run all tests
cd build && ctest

# Run with verbose output
cd build && ctest --verbose

# Run with output on failure
cd build && ctest --output-on-failure
```

## Test Coverage

When building with GCC or Clang, coverage instrumentation is automatically enabled. Generate coverage reports:

```bash
# After running tests
cd build
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/vcpkg_installed/*' '*/test/*' --output-file coverage_filtered.info
genhtml coverage_filtered.info --output-directory coverage_html
```

## Test Organization

```
test/
├── main.cpp              # Main test runner (doctest)
├── scratchpad.cpp        # Component model ABI tests
├── host-util.hpp         # Test utilities
├── host-util.cpp         # Host function helpers
├── test_grammar.cpp      # WIT grammar parser tests
├── CMakeLists.txt        # Test build configuration
└── README.md             # This file
```

VS Code launch configurations are available in `.vscode/launch.json` for running and debugging all tests.

## Dependencies

- **doctest**: C++ testing framework (from vcpkg)
- **ICU**: Unicode string handling (from vcpkg)
- **ANTLR4**: Grammar parser runtime (from vcpkg, for grammar tests)

## Continuous Integration

Tests are designed to run in CI/CD environments:

```yaml
# Example GitHub Actions
- name: Run Tests
  run: |
    cmake -B build
    cmake --build build
    cd build && ctest --output-on-failure

- name: Run Grammar Tests (if enabled)
  run: |
    cmake -B build -DBUILD_GRAMMAR=ON
    cmake --build build --target test-wit-grammar
    cd build && ctest -R wit-grammar-test --output-on-failure
```

## Adding New Tests

### Component Model Tests

Add test cases to `scratchpad.cpp` using doctest:

```cpp
TEST_CASE("My new test") {
    // Your test code
    CHECK(expected == actual);
}
```

### Grammar Tests

Grammar tests automatically pick up all `.wit` files in the test directory. To test additional files:

```bash
./build/test/test-wit-grammar --directory /path/to/wit/files
```

## Related Documentation

- [Main README](../README.md) - Project overview
- [Grammar README](../grammar/README.md) - Grammar documentation
- [Component Model Specification](../ref/component-model/design/mvp/CanonicalABI.md)
- [WIT Specification](../ref/component-model/design/mvp/WIT.md)
