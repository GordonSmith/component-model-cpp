# Resource Examples

This directory contains comprehensive examples demonstrating WebAssembly Component Model resource management in C++.

## Overview

The Component Model defines two types of resources:
- **Own Resources (`own_t<T>`)**: Exclusive ownership semantics with move-only behavior
- **Borrow Resources (`borrow_t<T>`)**: Shared reference semantics with copy behavior

## Examples

### 1. `simple_resource_demo.cpp`

A focused demonstration of core resource concepts:

```bash
# Build and run
cd build
make simple-resource-demo
./samples/wamr/resources/simple-resource-demo

# Run specific tests
./samples/wamr/resources/simple-resource-demo --basic      # Basic resource operations
./samples/wamr/resources/simple-resource-demo --table      # Resource table management
./samples/wamr/resources/simple-resource-demo --canon      # Canon resource functions
./samples/wamr/resources/simple-resource-demo --lift-lower # Lift/lower operations
```

**What it demonstrates:**
- Creating `own_t` and `borrow_t` resources
- Resource destructors and cleanup
- Move semantics for owned resources
- Resource table handle allocation and reuse
- Canon resource functions (`canon_resource_new`, `canon_resource_drop`, `canon_resource_rep`)
- Lift/lower operations for resource serialization

### 2. `resource_examples.cpp`

A comprehensive example showing real-world usage patterns:

```bash
# Build and run
cd build
make resource-examples
./samples/wamr/resources/resource-examples

# Run specific scenarios
./samples/wamr/resources/resource-examples --table-only     # Resource table only
./samples/wamr/resources/resource-examples --resources-only # Resource operations only
```

**What it demonstrates:**
- File handle resources with proper cleanup
- Database connection resources
- Complex resource composition (multiple resources in functions)
- Resource borrowing across function boundaries
- Error handling with invalid resources
- Resource lifetime management

## Key Concepts Demonstrated

### Resource Types

```cpp
// Define resource types with destructors
ResourceType<FileHandle> file_rt(file_destructor, true);
ResourceType<DatabaseConnection> db_rt(db_destructor, true);

// Create owned resources (exclusive ownership)
own_t<FileHandle> owned_file(&file_rt, FileHandle("test.txt"));

// Create borrowed resources (shared reference)
borrow_t<FileHandle> borrowed_file(&file_rt, owned_file.get());
```

### Resource Semantics

```cpp
// Owned resources use move semantics
own_t<FileHandle> moved = std::move(owned_file);
// owned_file is now invalid, moved is valid

// Borrowed resources use copy semantics
borrow_t<FileHandle> copied = borrowed_file;
// Both copied and borrowed_file remain valid
```

### Resource Tables

```cpp
ResourceTable table;

// Add resources and get handles
uint32_t handle1 = table.add(FileHandle("file1.txt"));
uint32_t handle2 = table.add(DatabaseConnection("db", 42));

// Retrieve with type safety
auto file = table.get<FileHandle>(handle1);  // Returns optional<FileHandle>
auto wrong = table.get<DatabaseConnection>(handle1);  // Returns nullopt

// Remove and reuse handles
table.remove(handle1);
uint32_t new_handle = table.add(FileHandle("file2.txt"));
// new_handle == handle1 (handle reuse)
```

### Canon Resource Functions

```cpp
// Create new resource handle
uint32_t handle = resource::canon_resource_new(&rt, representation);

// Get resource representation
uint32_t rep = resource::canon_resource_rep(&rt, handle);

// Drop resource (calls destructor)
resource::canon_resource_drop(&rt, handle, true);
```

### Lift/Lower Operations

```cpp
// Lower resource to flat values (for WASM transfer)
auto flat_vals = resource::lower_flat(context, owned_resource);

// Lift flat values back to resource
CoreValueIter vi(flat_vals);
auto lifted = resource::lift_flat<own_t<T>>(context, vi);

// Store/load in memory
resource::store(context, owned_resource, memory_ptr);
auto loaded = resource::load<own_t<T>>(context, memory_ptr);
```

## Building

From the project root:

```bash
mkdir -p build
cd build
cmake ..
make simple-resource-demo resource-examples

# Run the examples
./samples/wamr/resources/simple-resource-demo
./samples/wamr/resources/resource-examples
```

## Expected Output

The examples will show:
1. Resource creation and destruction messages
2. Destructor calls when resources go out of scope
3. Resource table operations and handle management
4. Move/copy semantics in action
5. Canon function demonstrations
6. Lift/lower operation results

## Integration with WAMR

While these examples run standalone to demonstrate the C++ resource system, they can be integrated with WAMR for full host-guest resource sharing. The canon functions and lift/lower operations provide the foundation for:

- Passing resources between host C++ code and WASM guests
- Managing resource lifetimes across the host-guest boundary
- Implementing proper resource cleanup and error handling
- Supporting both synchronous and asynchronous resource operations

## Reference

These examples correspond to the test cases in:
- `test/resource_tests.cpp` - Comprehensive resource testing
- `test/main.cpp` - Basic resource type verification
- Python reference: `ref/component-model/design/mvp/canonical-abi/run_tests.py`

For implementation details, see:
- `include/cmcpp/resource.hpp` - Resource type definitions
- `include/cmcpp/traits.hpp` - Resource type traits and concepts
