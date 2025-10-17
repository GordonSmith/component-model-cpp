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

**📖 Detailed grammar testing instructions are included in this section.**

#### Requirements
- Java Runtime Environment (for ANTLR)
- CMake flag: `-DWIT_CODEGEN=ON`

#### Quick Start

**Linux/macOS:**
```bash
# 1. Configure CMake with grammar support
cmake -B build -DWIT_CODEGEN=ON

# 2. Build the grammar and test executable
cmake --build build --target test-wit-grammar

# 3. Run the tests
cd build && ctest -R wit-grammar-test --verbose
```

**Windows:**
```bash
# 1. Configure CMake with grammar support
cmake -B build -DWIT_CODEGEN=ON

# 2. Build the grammar and test executable (specify configuration)
cmake --build build --target test-wit-grammar --config Release

# 3. Run the tests (must specify -C configuration on Windows)
cd build && ctest -C Release -R wit-grammar-test --verbose
```

### 3. WIT Stub Generation

Generate C++ host function stubs from all WIT test files. Useful for testing the code generator and creating reference implementations.

**📖 Stub generation and validation guidance appears throughout this section.**

#### Quick Start

```bash
# Prerequisites: build wit-codegen tool
cmake --build build --target wit-codegen

# Generate stubs for all test files (Python - recommended)
cd test && ./generate_test_stubs.py

# Or use the bash script
cd test && ./generate_test_stubs.sh

# Verbose output (see what files are generated)
./generate_test_stubs.py -v

# Filter specific files
./generate_test_stubs.py -f "streams"

# Skip CMakeLists.txt generation (if you don't need them)
./generate_test_stubs.py --no-cmake
```

#### Individual Stub Compilation

Each generated stub includes its own `CMakeLists.txt` in a dedicated subdirectory for standalone compilation testing:

```bash
# Generate stubs with CMakeLists.txt (default, parallelized)
cmake --build build --target generate-test-stubs

# Navigate to a specific stub directory
cd build/test/generated_stubs/simple-functions

# Build the individual stub
cmake -S . -B build
cmake --build build
```

**Performance:** Generation is parallelized using all available CPU cores by default. For 199 WIT files:
- Sequential: ~20 seconds
- Parallel (32 cores): ~4 seconds (5x faster)

Control parallelization:
```bash
python test/generate_test_stubs.py -j 8        # Use 8 parallel jobs
python test/generate_test_stubs.py -j 1        # Sequential (for debugging)
python test/generate_test_stubs.py --no-cmake  # Skip CMakeLists.txt generation
```

**Structure:** Each stub gets its own directory with all files:
```
generated_stubs/
├── simple-functions/
│   ├── CMakeLists.txt
│   ├── simple-functions.hpp
│   ├── simple-functions_wamr.hpp
│   └── simple-functions_wamr.cpp
├── integers/
│   ├── CMakeLists.txt
│   ├── integers.hpp
│   ├── integers_wamr.hpp
│   └── integers_wamr.cpp
...
```

**Note:** Stubs with `_wamr.cpp` files require WAMR (WebAssembly Micro Runtime) headers. The generated CMakeLists.txt includes helpful comments about dependencies and will automatically find the local cmcpp headers.

#### Code Generation Validation

The framework also validates generated code by attempting to compile it:

```bash
# Generate AND compile stubs (validation test)
cmake --build build --target test-stubs-full
```

**⚠️ IMPORTANT**: Compilation failures are **expected and useful**! They indicate bugs in `wit-codegen` that need fixing. Detailed validation notes are included in the following section.

#### Generated Files

For each `.wit` file in the test suite:
- `<name>.hpp` - Host/guest function declarations
- `<name>_wamr.hpp` - WAMR host wrappers with automatic marshaling
- `<name>_wamr.cpp` - WAMR native symbol registration

**Example output:**
```
generated_stubs/
├── floats.hpp
├── floats_wamr.hpp
├── floats_wamr.cpp
├── streams.hpp
├── streams_wamr.hpp
└── streams_wamr.cpp
```

#### Use Cases
- **Testing**: Verify the full toolchain (grammar → parser → codegen) works end-to-end
- **Reference**: See how WIT features map to C++ types and WAMR integration
- **Development**: Quick starting point for implementing Component Model features

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
cmake -B build -DWIT_CODEGEN=ON
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
├── main.cpp                      # Main test runner (doctest)
├── scratchpad.cpp                # Component model ABI tests
├── host-util.hpp                 # Test utilities
├── host-util.cpp                 # Host function helpers
├── test_grammar.cpp              # WIT grammar parser tests
├── generate_test_stubs.py        # Python stub generator (recommended)
├── generate_test_stubs.sh        # Bash stub generator
├── CMakeLists.txt                # Test build configuration
├── README.md                     # This file (overview)
├── TESTING_GRAMMAR.md            # Detailed grammar testing guide
├── STUB_GENERATION.md            # Detailed stub generation guide
└── STUB_GENERATION_QUICKREF.md  # Quick reference for stub generation
```

VS Code launch configurations are available in `.vscode/launch.json` for running and debugging all tests.

## Complete Workflow Example

Here's a complete example of the test workflow from WIT file to generated stubs:

```bash
# 1. Initialize the project
git clone <repo> && cd component-model-cpp
git submodule update --init --recursive

# 2. Build with grammar support
cmake -B build -DWIT_CODEGEN=ON
cmake --build build

# 3. Run grammar tests to validate WIT parsing
cd build && ctest -R wit-grammar-test --verbose

# 4. Generate stubs from all test WIT files
cd ../test
./generate_test_stubs.py -v

# 5. Check the generated stubs
ls generated_stubs/
cat generated_stubs/floats.hpp

# 6. (Optional) Use generated stubs in your project
cp generated_stubs/floats* ../my_project/
```

**Result:** You now have:
- ✅ Validated WIT parser working on 95+ test files
- ✅ Generated C++ host function stubs with WAMR integration
- ✅ Ready-to-use code for Component Model development

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
    cmake -B build -DWIT_CODEGEN=ON
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
- [Grammar README](https://github.com/GordonSmith/component-model-cpp/blob/main/grammar/README.md) - Grammar documentation
- [Component Model Specification](https://github.com/WebAssembly/component-model/blob/main/design/mvp/CanonicalABI.md)
- [WIT Specification](https://github.com/WebAssembly/component-model/blob/main/design/mvp/WIT.md)
