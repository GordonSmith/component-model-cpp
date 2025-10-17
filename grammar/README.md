# ANTLR Grammar Code Generation

This directory contains the ANTLR grammar files for parsing WebAssembly Interface Types (WIT) format.

## Overview

The grammar is based on the official [WebAssembly Component Model WIT specification](https://github.com/WebAssembly/component-model/blob/main/design/mvp/WIT.md).

## Prerequisites

- **Java Runtime**: ANTLR requires Java to run the code generator
- **CMake**: Used to automate the build process
- **ANTLR4**: Automatically downloaded via CMake (version 4.13.2)

## Building

### Enable Grammar Generation

The grammar generation is controlled by the `BUILD_WIT_CODEGEN` CMake option (enabled by default):

```bash
cd build
cmake -DBUILD_WIT_CODEGEN=ON ..
```

This will:
1. Check if Java is available
2. Automatically download the ANTLR jar file if not present
3. Configure the `generate-grammar` target

### Generate Code

To generate TypeScript code from the grammar files:

```bash
cmake --build . --target generate-grammar
```

Or using make:

```bash
make generate-grammar
```

### Output Location

The generated C++ files are placed in the build directory (not shipped with the library):
```
build/grammar/grammar/
├── WitLexer.h
├── WitLexer.cpp
├── WitParser.h
├── WitParser.cpp
├── WitVisitor.h
├── WitVisitor.cpp
├── WitBaseVisitor.h
└── WitBaseVisitor.cpp
```

## Testing

### Grammar Test Suite

A comprehensive test suite validates the grammar against all official WIT test files from the [wit-bindgen repository](https://github.com/bytecodealliance/wit-bindgen).

The test suite is located in the `test/` directory and provides:
- Validation against 79 official WIT files from wit-bindgen
- Complete coverage of all WIT features
- Automated testing via CTest

For detailed testing documentation, see [test/README.md](../test/README.md#2-wit-grammar-tests-test_grammarcpp).

#### Quick Test

```bash
# Build and run tests
cmake -B build -DBUILD_WIT_CODEGEN=ON
cmake --build build --target test-wit-grammar
ctest -R wit-grammar-test --verbose

# Or run directly
./build/test/test-wit-grammar --verbose

# Or use VS Code launch configurations (see .vscode/launch.json)
```

#### Test Coverage

The test suite validates the grammar against 79 WIT files including:
- Basic types (integers, floats, strings, chars)
- Complex types (records, variants, enums, flags)
- Resources (with constructors, methods, static functions)
- Functions (sync and async)
- Futures and streams
- Interfaces and worlds
- Package declarations with versions
- Use statements and imports/exports
- Feature gates (@unstable, @since, @deprecated)
- Error contexts
- Real-world WASI specifications (wasi-clocks, wasi-filesystem, wasi-http, wasi-io)

#### Test Output

The test executable reports:
- Total number of WIT files found
- Number of successfully parsed files
- Number of failed files with detailed error messages
- Exit code 0 for success, 1 for failures

Example output:
```
WIT Grammar Test Suite
======================
Test directory: ../ref/wit-bindgen/tests/codegen

Found 79 WIT files

✓ Successfully parsed: allow-unused.wit
✓ Successfully parsed: async-trait-function.wit
✓ Successfully parsed: char.wit
...

======================
Test Results:
  Total files:  79
  Successful:   79
  Failed:       0

✓ All tests passed!
```

### Command Line Options

The test executable supports several options:

```bash
# Show help
./build/test/test-wit-grammar --help

# Use verbose output (shows each file as it's parsed)
./build/test/test-wit-grammar --verbose

# Specify a different test directory
./build/test/test-wit-grammar --directory /path/to/wit/files
```

## Manual Generation

You can also generate the code manually using the downloaded jar:

```bash
cd grammar
java -jar ../antlr-4.13.2-complete.jar \
    -Dlanguage=Cpp \
    -o ../build/grammar/grammar \
    -visitor \
    -no-listener \
    -Xexact-output-dir \
    ./*.g4
```

Note: The double `grammar` in the path is intentional - first is the CMake subdirectory, second is the output folder.

## Using the Generated Files

The generated C++ source files can be used by including them directly in your tool's build. Consumers must:
1. Link to the `antlr4_shared` (or `antlr4_static`) library from vcpkg
2. Add the ANTLR4 runtime include directory and grammar output directory to their include paths
3. Compile the generated `.cpp` files as part of their target

See `tools/wit-codegen/CMakeLists.txt` and `test/CMakeLists.txt` for complete examples.

## Grammar Files

- `Wit.g4`: Main grammar file for WebAssembly Interface Types

## Configuration

The following CMake variables can be customized:

- `ANTLR_VERSION`: ANTLR version to use (default: 4.13.2)
- `ANTLR_JAR_PATH`: Path to ANTLR jar file (default: `../antlr-${ANTLR_VERSION}-complete.jar`)
- `ANTLR_GRAMMAR_DIR`: Directory containing .g4 files (default: current directory)
- `ANTLR_OUTPUT_DIR`: Output directory for generated code (default: `${CMAKE_CURRENT_BINARY_DIR}/grammar`)

Example:

```bash
cmake -DBUILD_WIT_CODEGEN=ON \
      -DANTLR_VERSION=4.13.2 \
      -DANTLR_OUTPUT_DIR=/custom/path \
      ..
```

## Example: Using the Grammar in Your Tool

### CMakeLists.txt Setup

Tools that need to parse WIT files should link directly to the ANTLR4 runtime and include the generated source files:

```cmake
# Find ANTLR4 runtime from vcpkg
find_package(antlr4-runtime CONFIG REQUIRED)

# Determine which library to use (shared or static)
if(TARGET antlr4_shared)
    set(ANTLR4_LIBRARY antlr4_shared)
elseif(TARGET antlr4_static)
    set(ANTLR4_LIBRARY antlr4_static)
endif()

# Get the grammar output directory from the grammar target
get_target_property(ANTLR_OUTPUT_DIR generate-grammar BINARY_DIR)
set(ANTLR_OUTPUT_DIR "${ANTLR_OUTPUT_DIR}/grammar")

# List generated source files explicitly
set(ANTLR_GENERATED_SOURCES
    ${ANTLR_OUTPUT_DIR}/WitLexer.cpp
    ${ANTLR_OUTPUT_DIR}/WitParser.cpp
    ${ANTLR_OUTPUT_DIR}/WitVisitor.cpp
    ${ANTLR_OUTPUT_DIR}/WitBaseVisitor.cpp
)

# Create your tool executable
add_executable(my_wit_tool 
    main.cpp
    ${ANTLR_GENERATED_SOURCES}
)

# Link to ANTLR4 runtime
target_link_libraries(my_wit_tool PRIVATE 
    ${ANTLR4_LIBRARY}
)

# Add include directories
target_include_directories(my_wit_tool PRIVATE
    ${ANTLR_OUTPUT_DIR}  # For grammar headers
    ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include/antlr4-runtime
)

# Ensure grammar is generated before building
add_dependencies(my_wit_tool generate-grammar)
```

### Example C++ Code

```cpp
#include <antlr4-runtime.h>
#include "grammar/WitLexer.h"
#include "grammar/WitParser.h"
#include "grammar/WitBaseVisitor.h"
#include <fstream>
#include <iostream>

using namespace antlr4;

// Custom visitor to process the parse tree
class MyWitVisitor : public WitBaseVisitor {
public:
    std::any visitPackageDecl(WitParser::PackageDeclContext *ctx) override {
        // Process package declaration
        std::cout << "Package: " << ctx->getText() << std::endl;
        return visitChildren(ctx);
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <wit-file>" << std::endl;
        return 1;
    }
    
    // Read WIT file
    std::ifstream stream(argv[1]);
    if (!stream) {
        std::cerr << "Failed to open file: " << argv[1] << std::endl;
        return 1;
    }
    
    ANTLRInputStream input(stream);
    
    // Create lexer and parser
    WitLexer lexer(&input);
    CommonTokenStream tokens(&lexer);
    WitParser parser(&tokens);
    
    // Parse and visit
    tree::ParseTree* tree = parser.witFile();
    
    // Check for errors
    if (parser.getNumberOfSyntaxErrors() > 0) {
        std::cerr << "Parse errors encountered" << std::endl;
        return 1;
    }
    
    // Visit the parse tree
    MyWitVisitor visitor;
    visitor.visit(tree);
    
    return 0;
}
```

For a complete working example, see `tools/wit-codegen/` which uses this exact pattern.

## vcpkg Integration

The project uses vcpkg to manage the ANTLR4 C++ runtime library. The ANTLR jar file for code generation is automatically downloaded during CMake configuration.

To manually install ANTLR4 runtime via vcpkg:

```bash
# Already included in vcpkg.json
./vcpkg/vcpkg install antlr4
```

Note: The vcpkg ANTLR4 package provides the C++ runtime library, not the Java-based code generator. The jar file is downloaded separately by CMake.

## Troubleshooting

### Java Not Found

If CMake reports that Java is not found:

```bash
# Install Java (Ubuntu/Debian)
sudo apt-get install default-jre

# Or using SDKMAN (recommended for managing Java versions)
curl -s "https://get.sdkman.io" | bash
sdk install java
```

### ANTLR Download Failed

If the automatic download fails, manually download the jar:

```bash
curl -L -o antlr-4.13.2-complete.jar \
  https://www.antlr.org/download/antlr-4.13.2-complete.jar
```

Place it in the project root directory.

### Grammar Changes Not Reflected

The build system tracks dependencies on .g4 files. If changes aren't being picked up:

```bash
# Clean and rebuild
cmake --build . --target clean
cmake --build . --target generate-grammar
```

## References

- [ANTLR 4 Documentation](https://www.antlr.org/)
- [ANTLR 4 Download](https://www.antlr.org/download.html)
- [WebAssembly Component Model](https://github.com/WebAssembly/component-model)
- [WIT Specification](https://github.com/WebAssembly/component-model/blob/main/design/mvp/WIT.md)
