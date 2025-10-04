# WIT Code Generator Tool

This directory contains the `wit-codegen` tool that uses the generated ANTLR WIT grammar library.

## wit-codegen

A code generator that reads WIT files and generates C++ host function bindings using cmcpp types.

**Usage:**
```bash
./build/tools/wit-codegen/wit-codegen <wit-file> <output-prefix>
```

**Example:**
```bash
./build/tools/wit-codegen/wit-codegen test-example.wit host_functions
```

This generates:
- `host_functions.hpp` - Header with function declarations
- `host_functions.cpp` - Implementation stubs with TODO comments  
- `host_functions_bindings.cpp` - WAMR NativeSymbol bindings

See [docs/wit-codegen.md](../docs/wit-codegen.md) for detailed documentation.

## Structure

```
tools/
└── wit-codegen/
    ├── CMakeLists.txt
    └── wit-codegen.cpp    # wit-codegen tool
```

## Building

The tool is built automatically when you enable grammar generation:

```bash
cd build
cmake -DBUILD_GRAMMAR=ON ..
cmake --build . --target wit-codegen
```

Or build everything:
```bash
cmake --build .
```

## Implementation Notes

- The tool links directly against ANTLR4 runtime (platform-specific: `antlr4_shared` on Windows, `antlr4_static` on Linux)
- Generated ANTLR source files are compiled into the tool
- Depends on the `generate-grammar` CMake target to ensure grammar code is generated first
- Generated grammar headers are available via:
  - `#include "grammar/WitLexer.h"`
  - `#include "grammar/WitParser.h"`
  - `#include "grammar/WitBaseVisitor.h"`
- The grammar code is generated in the build tree and not installed
- Uses `WitBaseVisitor` to implement custom visitor for parse tree traversal

