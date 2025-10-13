# Quick Reference: WIT Test Stub Generation

## Generate All Stubs

```bash
# Python (recommended)
cd test && ./generate_test_stubs.py

# Bash
cd test && ./generate_test_stubs.sh
```

## Common Options

```bash
# Verbose output (see generated files)
./generate_test_stubs.py -v

# Filter specific files
./generate_test_stubs.py -f "streams"
./generate_test_stubs.py -f "wasi-"

# Custom output directory
./generate_test_stubs.py -o my_stubs

# Full help
./generate_test_stubs.py --help
```

## Prerequisites

```bash
# Build wit-codegen first
cmake --build build --target wit-codegen

# Ensure submodules are initialized
git submodule update --init --recursive
```

## Generated Files

For each `.wit` file:
- `<name>.hpp` - Host/guest function declarations
- `<name>_wamr.hpp` - WAMR host wrappers
- `<name>_wamr.cpp` - WAMR native symbol registration

## Example Output

```
generated_stubs/
├── floats.hpp
├── floats_wamr.hpp
└── floats_wamr.cpp
```

## See Also

- [STUB_GENERATION.md](STUB_GENERATION.md) - Complete documentation
- [TESTING_GRAMMAR.md](TESTING_GRAMMAR.md) - Grammar testing guide
- [../tools/wit-codegen/README.md](../tools/wit-codegen/README.md) - Code generator docs
