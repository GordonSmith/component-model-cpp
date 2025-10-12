# Code Generation Validation Framework

## Overview

This framework validates the entire WIT-to-C++ code generation pipeline by:
1. Generating C++ stubs from WIT test files using `wit-codegen`
2. Attempting to compile the generated code
3. **Reporting compilation failures as bugs to fix**

**IMPORTANT**: Compilation failures are **expected and useful** - they indicate bugs in `wit-codegen` that need to be fixed!

**NEW**: Support for **incremental testing** - test subsets of stubs to fix issues progressively. See [INCREMENTAL_TESTING.md](INCREMENTAL_TESTING.md) for details.

## Architecture

```
WIT Test Files (95+)
        ↓
   wit-codegen
        ↓
Generated C++ Stubs
        ↓
   C++ Compiler
        ↓
[Success] ✓ Code generation works
[Failure] ✗ Bug in code generator - needs fixing!
```

## CMake Targets

Located in `test/StubGenerationTests.cmake`:

### `generate-test-stubs`
Generates C++ stubs from all WIT test files.

```bash
cmake --build build --target generate-test-stubs
```

**Output**: `build/test/generated_stubs/*.{hpp,cpp}`

### `validate-test-stubs` 
Compiles ALL generated stubs to validate code generation.

```bash
cmake --build build --target validate-test-stubs
```

**Expected**: May fail! That's the point - failures show bugs.

### `validate-test-stubs-basic` ⭐
Compiles only basic types (floats, integers, strings, char) - should always pass.

```bash
cmake --build build --target validate-test-stubs-basic
```

### `validate-test-stubs-composite`
Compiles only composite types (records, variants, enums, flags).

```bash
cmake --build build --target validate-test-stubs-composite
```

### `validate-test-stubs-async`
Compiles only async types (streams, futures) - expected to fail initially.

```bash
cmake --build build --target validate-test-stubs-async
```

### `validate-test-stubs-incremental`
Compiles all types except async - for progressive fixing.

```bash
cmake --build build --target validate-test-stubs-incremental
```

### `test-stubs-full`
Combined: generate + validate ALL in one step.

```bash
cmake --build build --target test-stubs-full
```

### `clean-test-stubs`
Remove generated stub files.

```bash
cmake --build build --target clean-test-stubs
```

## Python Validation Script

For more control, use the Python script directly:

```bash
# Test specific groups
cd test && ./validate_stubs.py --group basic
./validate_stubs.py --group composite -v

# Test specific files
./validate_stubs.py floats integers -v

# Test all except async
./validate_stubs.py --group all --exclude async

# List available options
./validate_stubs.py --help
./validate_stubs.py --list-groups
```

**📖 See [INCREMENTAL_TESTING.md](INCREMENTAL_TESTING.md) for complete guide to incremental testing.**

## What Gets Tested

The framework attempts to compile stubs for:

### Basic Types (Should Always Work)
- ✅ `floats` - f32, f64 types
- ✅ `integers` - u8-u64, s8-s64 types  
- ✅ `strings` - String handling
- ✅ `char` - Character type

### Collections
- ✅ `lists` - List/vector types

### Composite Types
- ✅ `records` - Struct types
- ✅ `variants` - Union types
- ✅ `enums` - Enum types
- ✅ `flags` - Bitset types
- ✅ `tuples` - Tuple types

### Option/Result Types
- ✅ `options` - Optional values
- ✅ `results` - Result<T, E> types

### Function Features
- ✅ `multi-return` - Multiple return values
- ✅ `conventions` - Naming conventions

### Complex Features (Likely to Expose Issues)
- ⚠️ `resources` - Resource handles
- ⚠️ `streams` - Stream types (async)
- ⚠️ `futures` - Future types (async)

## Understanding Failures

### Compilation Error Example
```
error: 'string_stream' is not a member of 'host::streams'
   48 |     host_function("string-stream", host::streams::string_stream),
      |                                                   ^~~~~~~~~~~~~
```

**What this means:**
- `wit-codegen` generated a WAMR binding that references `host::streams::string_stream`
- But it forgot to generate the actual function declaration in the header
- **This is a bug in wit-codegen that needs fixing**

### Common Issue Patterns

#### 1. Missing Function Declarations
**Symptom**: WAMR bindings reference functions that don't exist in header  
**Root cause**: `wit-codegen` not generating declarations for certain WIT function types  
**Example**: Async functions (streams, futures) often hit this

#### 2. Type Mismatches  
**Symptom**: Function signature doesn't match between header and bindings  
**Root cause**: Type mapping bug in `wit-codegen`  
**Example**: Resource types might map incorrectly

#### 3. Missing Type Definitions
**Symptom**: Undefined type names  
**Root cause**: `wit-codegen` not generating typedef/struct for WIT types  
**Example**: Records or variants not being generated

#### 4. Namespace Issues
**Symptom**: Name not found in expected namespace  
**Root cause**: Incorrect namespace generation from WIT package structure  
**Example**: `foo:bar/baz` mapping to wrong C++ namespace

## Workflow for Fixing Issues

### 1. Run the Test
```bash
cmake --build build --target test-stubs-full
```

### 2. Identify the Failing File
Look for the error message showing which generated file failed:
```
/build/test/generated_stubs/streams_wamr.cpp:48:51: error: ...
```

### 3. Examine the Generated Code
```bash
# Look at the generated header
cat build/test/generated_stubs/streams.hpp

# Look at the WAMR bindings
cat build/test/generated_stubs/streams_wamr.cpp

# Look at the original WIT file
cat ref/wit-bindgen/tests/codegen/streams.wit
```

### 4. Compare What Was Generated vs. What Should Be
- Check if function exists in header but not in bindings (or vice versa)
- Check if types match between files
- Check if WIT syntax is being parsed correctly

### 5. Fix wit-codegen
Edit `tools/wit-codegen/wit-codegen.cpp` to fix the code generation bug.

### 6. Rebuild and Retest
```bash
cmake --build build --target wit-codegen
cmake --build build --target test-stubs-full
```

### 7. Repeat Until All Tests Pass
When all stubs compile successfully, `wit-codegen` is fully working!

## Current Known Issues

As of this framework's implementation, here are known failures:

### 1. Async Functions Not Generated
**Files affected**: `streams.wit`, `futures.wit`  
**Issue**: Functions with `stream<T>` or `future<T>` parameters/returns are not being generated in headers  
**Status**: 🔴 Needs fixing in wit-codegen

**Example error:**
```
error: 'string_stream' is not a member of 'host::streams'
```

**Fix needed**: Update `WitInterfaceVisitor` in `wit-codegen.cpp` to handle async function types.

## Integration with CI/CD

The validation test can be added to CI pipelines:

```yaml
# GitHub Actions example
- name: Validate Code Generation
  run: |
    cmake -B build -DBUILD_GRAMMAR=ON
    cmake --build build --target test-stubs-full
  continue-on-error: true  # Don't fail CI, just report
  
- name: Report Issues
  if: failure()
  run: |
    echo "Code generation has issues - see logs above"
    echo "This is expected during development"
```

Or make it a required check once `wit-codegen` is mature:

```yaml
- name: Validate Code Generation
  run: |
    cmake -B build -DBUILD_GRAMMAR=ON
    cmake --build build --target test-stubs-full
  # Fails CI if code generation is broken
```

## Development Workflow

### Day-to-Day Usage

1. **Working on wit-codegen?** Run validation frequently:
   ```bash
   # Quick iteration loop
   cmake --build build --target wit-codegen && \
   cmake --build build --target test-stubs-full
   ```

2. **Added new WIT feature support?** Add it to `TEST_STUBS_TO_COMPILE` in `StubGenerationTests.cmake`

3. **Found a new test case?** Add the WIT filename to the test list

### Debugging Tips

**See exactly what's being generated:**
```bash
# Generate stubs with verbose output
cd test
./generate_test_stubs.py -v -f "streams"

# Inspect the output
ls -la build/test/generated_stubs/streams*
```

**Test just one WIT file:**
```bash
# Generate only one stub
./generate_test_stubs.py -f "floats"

# Try to compile it manually
g++ -std=c++20 -I ../include -I build/test/generated_stubs \
    -c build/test/generated_stubs/floats_wamr.cpp
```

**Compare with working examples:**
```bash
# See what works
cat build/test/generated_stubs/integers.hpp  # ✓ Working

# Compare to what doesn't
cat build/test/generated_stubs/streams.hpp   # ✗ Broken
```

## Success Criteria

The code generation pipeline is considered **validated** when:

✅ All test stubs in `TEST_STUBS_TO_COMPILE` compile without errors  
✅ Generated code follows C++20 standards  
✅ Generated code correctly uses cmcpp types  
✅ WAMR bindings match generated function signatures  
✅ Namespaces correctly reflect WIT package structure  

## Future Enhancements

- [ ] **Individual file targets**: Compile each stub separately to isolate failures
- [ ] **Diff testing**: Compare generated output against golden files
- [ ] **Runtime testing**: Not just compile, but execute generated bindings
- [ ] **Coverage metrics**: Track % of WIT features successfully generated
- [ ] **Automatic issue filing**: Parse errors and suggest fixes

## Related Documentation

- [STUB_GENERATION.md](STUB_GENERATION.md) - Stub generation scripts
- [TESTING_GRAMMAR.md](TESTING_GRAMMAR.md) - Grammar validation
- [../tools/wit-codegen/README.md](../tools/wit-codegen/README.md) - Code generator docs
- [README.md](README.md) - Test suite overview

## Summary

This validation framework is a **critical quality gate** for the code generation pipeline:

- **Detects bugs early** before they reach users
- **Validates end-to-end** from WIT to compiled C++
- **Provides clear feedback** on what needs fixing
- **Enables confident development** of new codegen features

**Remember**: Failures are features, not bugs! They tell us exactly what needs work.
