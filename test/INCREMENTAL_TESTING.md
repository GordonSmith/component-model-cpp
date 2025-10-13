# Incremental Code Generation Testing

## Quick Reference

### Test by Group (Recommended)

```bash
# Test only basic types (should pass)
cd test && ./validate_stubs.py --group basic

# Test composite types  
./validate_stubs.py --group composite

# Test async types (expected to fail initially)
./validate_stubs.py --group async

# Test everything except async
./validate_stubs.py --group all --exclude async
```

### Test Specific Files

```bash
# Test individual stubs
cd test && ./validate_stubs.py floats integers strings

# Test with verbose error output
./validate_stubs.py floats -v
```

### Using CMake Targets

```bash
# Test basic types
cmake --build build --target validate-test-stubs-basic

# Test composite types
cmake --build build --target validate-test-stubs-composite

# Test all except async
cmake --build build --target validate-test-stubs-incremental

# Test async (expected failures)
cmake --build build --target validate-test-stubs-async
```

## Available Groups

| Group | Files | Status |
|-------|-------|--------|
| `basic` | floats, integers, strings, char | ✅ Should pass |
| `collections` | lists, tuples | ⚠️ Some issues |
| `composite` | records, variants, enums, flags | ⚠️ Some issues |
| `options` | options, results | ⚠️ May not generate |
| `functions` | multi-return, conventions | ⚠️ Some issues |
| `resources` | resources | ⚠️ Known issues |
| `async` | streams, futures | ❌ Known failures |

## Workflow for Fixing Issues

### 1. Start with Basic Types
```bash
./validate_stubs.py --group basic
```
**Expected**: All pass ✅

### 2. Move to Collections
```bash
./validate_stubs.py --group collections -v
```
**Fix issues** in wit-codegen, then rerun

### 3. Test Composite Types
```bash
./validate_stubs.py --group composite -v
```
**Fix issues**, rebuild wit-codegen, rerun

### 4. Test Functions
```bash
./validate_stubs.py --group functions -v
```

### 5. Test Resources
```bash
./validate_stubs.py --group resources -v
```

### 6. Finally, Async (Hardest)
```bash
./validate_stubs.py --group async -v
```

### 7. Full Validation
```bash
./validate_stubs.py --group all
# Or use CMake
cmake --build build --target validate-test-stubs
```

## Understanding Output

### Success
```
[1/4] floats                         ... ✓
```
Generated code compiles successfully!

### Failure
```
[2/4] lists                          ... ✗
  - lists
    error: 'monostate' is not a member of 'cmcpp'
```
Code generation bug - needs fixing in wit-codegen

### Not Generated
```
[3/4] tuples                         ... ⊘ (not generated)
```
WIT file didn't produce output (may be world-only or empty)

## Common Issues and Fixes

### Issue: `'monostate' is not a member of 'cmcpp'`
**Files**: lists, variants, resources  
**Fix**: Add `using monostate = std::monostate;` to cmcpp traits or use `std::monostate` directly

### Issue: `variable or field 'foo' declared void`
**Files**: records, conventions  
**Fix**: wit-codegen not generating proper function signatures for empty records

### Issue: `expected identifier before '%' token`
**Files**: flags  
**Fix**: wit-codegen generating invalid C++ identifiers (reserved chars)

### Issue: Compilation timeout
**Files**: streams, futures  
**Fix**: Likely infinite template recursion or massive generated code

## Advanced Usage

### Test a Single Stub with Full Debugging
```bash
# Generate just one stub
./generate_test_stubs.py -f "floats"

# Compile it manually
g++ -std=c++20 -c \
  -I ../include \
  -I ../build/vcpkg_installed/x64-linux/include \
  -I ../build/test/generated_stubs \
  ../build/test/generated_stubs/floats_wamr.cpp \
  -o /tmp/floats.o

# Or use the validator
./validate_stubs.py floats -v
```

### Compare Working vs Broken
```bash
# See what works
./validate_stubs.py floats integers -v > working.txt

# See what's broken
./validate_stubs.py lists records -v > broken.txt

# Compare generated code
diff ../build/test/generated_stubs/floats.hpp \
     ../build/test/generated_stubs/lists.hpp
```

### Track Progress
```bash
# Create baseline
./validate_stubs.py --group all > baseline.txt

# After fixes, compare
./validate_stubs.py --group all > current.txt
diff baseline.txt current.txt
```

## Python Script Options

```
usage: validate_stubs.py [-h] [-g {all,basic,collections,composite,options,functions,resources,async}]
                         [--exclude {basic,collections,composite,options,functions,resources,async}]
                         [-v] [-d STUB_DIR] [-i INCLUDE_DIR] [--list-groups] [--list-stubs]
                         [stubs ...]

Options:
  stubs                 Specific stub names to test
  -g, --group GROUP     Test a predefined group
  --exclude GROUP       Exclude a group (when using --group all)
  -v, --verbose         Show compilation errors
  -d, --stub-dir PATH   Generated stubs directory
  -i, --include-dir PATH Include directory (cmcpp headers)
  --list-groups         List available groups
  --list-stubs          List available stub files
```

## Integration with Development

### Quick Iteration Loop
```bash
# 1. Edit wit-codegen.cpp
vim tools/wit-codegen/wit-codegen.cpp

# 2. Rebuild code generator
cmake --build build --target wit-codegen

# 3. Regenerate stubs
cmake --build build --target generate-test-stubs

# 4. Test the group you're fixing
cd test && ./validate_stubs.py --group composite -v

# 5. Repeat until passing
```

### One-Liner for Quick Testing
```bash
# Rebuild, regenerate, and test in one command
cmake --build build --target wit-codegen && \
cmake --build build --target generate-test-stubs && \
./test/validate_stubs.py --group composite -v
```

## Success Criteria by Group

### Basic (Target: 100%)
- ✅ floats
- ✅ integers  
- ✅ strings
- ✅ char

### Collections (Target: 100%)
- ⚠️ lists (monostate issue)
- ⚠️ tuples (not generating?)

### Composite (Target: 100%)
- ⚠️ records (empty record issue)
- ⚠️ variants (monostate issue)
- ⚠️ enums (not generating?)
- ⚠️ flags (invalid identifier issue)

### Functions (Target: 100%)
- ✅ multi-return
- ⚠️ conventions (empty param issue)

### Resources (Target: 80%+)
- ⚠️ resources (monostate + complex issues)

### Async (Target: 50%+)
- ❌ streams (timeout - huge issues)
- ❌ futures (timeout - huge issues)

## See Also

- [CODEGEN_VALIDATION.md](CODEGEN_VALIDATION.md) - Full validation framework docs
- [STUB_GENERATION.md](STUB_GENERATION.md) - Stub generation details
- [../tools/wit-codegen/README.md](../tools/wit-codegen/README.md) - Code generator docs
