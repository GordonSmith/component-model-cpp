# Quick Start Guide

This guide will walk you through creating a complete WebAssembly Component Model application using the Component Model C++ library. You'll create a simple calculator interface that demonstrates bidirectional communication between a C++ host and a WebAssembly guest.

## Prerequisites

- CMake 3.22 or later
- C++20 compatible compiler (GCC 11+, Clang 14+, MSVC 2022+)
- [Rust toolchain](https://rustup.rs/) (for `wit-bindgen` and `wasm-tools`)
- WASI SDK (automatically installed via vcpkg)

Install Rust tools:
```bash
cargo install wit-bindgen-cli wasm-tools
```

## Project Structure

We'll create the following structure:

```
calculator-app/
├── CMakeLists.txt
├── calculator.wit          # Interface definition
├── guest/                  # WebAssembly guest code
│   ├── CMakeLists.txt
│   └── main.cpp
└── host/                   # C++ host application
    ├── CMakeLists.txt
    ├── main.cpp
    ├── host_impl.cpp
    └── generated/          # Generated bindings (auto-created)
```

## Step 1: Define the WIT Interface

Create `calculator.wit` to define the interface between host and guest:

```wit
package math:calculator@1.0.0;

/// Core arithmetic operations provided by the host
interface operations {
    /// Multiply two numbers (provided by host)
    multiply: func(a: f64, b: f64) -> f64;
    
    /// Divide two numbers (provided by host)
    /// Returns result on success, error message on failure
    divide: func(a: f64, b: f64) -> result<f64, string>;
}

/// Computation functions exported by the guest
interface compute {
    /// Calculate the area of a circle given radius
    circle-area: func(radius: f64) -> f64;
    
    /// Calculate compound interest
    /// principal: initial amount
    /// rate: annual interest rate (as decimal, e.g., 0.05 for 5%)
    /// years: number of years
    compound-interest: func(principal: f64, rate: f64, years: u32) -> f64;
    
    /// Process a batch of numbers and return statistics
    record stats {
        count: u32,
        sum: f64,
        average: f64,
        min: f64,
        max: f64,
    }
    batch-stats: func(numbers: list<f64>) -> stats;
}

world calculator {
    import operations;
    export compute;
}
```

### Key WIT Concepts

- **Packages**: Organize interfaces with versioning (`math:calculator@1.0.0`)
- **Interfaces**: Group related functions
  - `operations`: Functions the **host implements** (guest imports)
  - `compute`: Functions the **guest implements** (guest exports)
- **Types**: Rich type system with primitives, composites, and error handling
  - Primitives: `u32`, `f64`, `string`, `bool`
  - Composites: `list<T>`, `record { ... }`, `result<T, E>`
- **World**: Defines the complete interface contract

## Step 2: Generate Guest Bindings and Build WASM

For detailed guidance on creating guest components in various languages, see the [Component Model Language Support Guide](https://component-model.bytecodealliance.org/language-support.html). This section demonstrates the C/C++ approach using `wit-bindgen`.

### Create Guest CMakeLists.txt

Create `guest/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.10)
project(calculator-guest)

# WASI SDK should be configured via toolchain file
set(CMAKE_EXECUTABLE_SUFFIX ".wasm")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -nostartfiles -fno-exceptions --sysroot=${WASI_SDK_PREFIX}/share/wasi-sysroot -Wl,--no-entry")

# Generate C bindings from WIT using wit-bindgen
add_custom_command(
    OUTPUT 
        ${CMAKE_CURRENT_BINARY_DIR}/calculator/calculator.c
        ${CMAKE_CURRENT_BINARY_DIR}/calculator/calculator.h
        ${CMAKE_CURRENT_BINARY_DIR}/calculator/calculator_component_type.o
    COMMAND wit-bindgen c --out-dir "${CMAKE_CURRENT_BINARY_DIR}/calculator" "${CMAKE_SOURCE_DIR}/calculator.wit"
    DEPENDS ${CMAKE_SOURCE_DIR}/calculator.wit
    COMMENT "Generating guest C bindings from WIT"
)

add_custom_target(generate-calculator-guest ALL
    DEPENDS 
        ${CMAKE_CURRENT_BINARY_DIR}/calculator/calculator.c
        ${CMAKE_CURRENT_BINARY_DIR}/calculator/calculator.h
)

# Build the guest WebAssembly module
add_executable(calculator
    main.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/calculator/calculator.c
)

target_include_directories(calculator PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/calculator
)

target_link_libraries(calculator
    ${CMAKE_CURRENT_BINARY_DIR}/calculator/calculator_component_type.o
)

# Convert core module to component using wasm-tools
find_program(WASM_TOOLS wasm-tools REQUIRED)

add_custom_command(
    TARGET calculator POST_BUILD
    COMMAND ${WASM_TOOLS} component new 
        ${CMAKE_CURRENT_BINARY_DIR}/calculator.wasm 
        -o ${CMAKE_CURRENT_BINARY_DIR}/calculator_component.wasm
    COMMENT "Converting core module to component"
)
```

### Implement Guest Functions

Create `guest/main.cpp`:

```cpp
#include "calculator.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>

// Import host functions (implemented by host)
__attribute__((import_module("math:calculator/operations"), import_name("multiply")))
extern double math_calculator_operations_multiply(double a, double b);

__attribute__((import_module("math:calculator/operations"), import_name("divide")))
extern bool math_calculator_operations_divide(double a, double b, double *ret, calculator_string_t *err);

// Export: Calculate circle area using host's multiply function
double exports_math_calculator_compute_circle_area(double radius)
{
    const double pi = 3.14159265358979323846;
    // Call host multiply: area = π * r * r
    double r_squared = math_calculator_operations_multiply(radius, radius);
    return math_calculator_operations_multiply(pi, r_squared);
}

// Export: Calculate compound interest
double exports_math_calculator_compute_compound_interest(double principal, double rate, uint32_t years)
{
    // A = P(1 + r)^n
    double multiplier = 1.0 + rate;
    double result = principal;
    
    for (uint32_t i = 0; i < years; ++i) {
        result = math_calculator_operations_multiply(result, multiplier);
    }
    
    return result;
}

// Export: Batch statistics calculation
void exports_math_calculator_compute_batch_stats(
    exports_math_calculator_compute_list_f64_t *numbers,
    exports_math_calculator_compute_stats_t *ret)
{
    if (!numbers || numbers->len == 0) {
        ret->count = 0;
        ret->sum = 0.0;
        ret->average = 0.0;
        ret->min = 0.0;
        ret->max = 0.0;
        return;
    }
    
    ret->count = numbers->len;
    ret->sum = 0.0;
    ret->min = numbers->ptr[0];
    ret->max = numbers->ptr[0];
    
    for (size_t i = 0; i < numbers->len; ++i) {
        double val = numbers->ptr[i];
        ret->sum += val;
        if (val < ret->min) ret->min = val;
        if (val > ret->max) ret->max = val;
    }
    
    ret->average = ret->sum / ret->count;
}
```

### Build the Guest

```bash
cd guest
cmake -DWASI_SDK_PREFIX=/path/to/wasi-sdk \
      -DCMAKE_TOOLCHAIN_FILE=/path/to/wasi-sdk/share/cmake/wasi-sdk.cmake \
      -B build
cmake --build build

# Result: build/calculator_component.wasm
```

**Note**: If you're using vcpkg, `WASI_SDK_PREFIX` will be automatically set. See the samples directory for vcpkg integration examples.

## Step 3: Generate Host Bindings

The Component Model C++ library includes `wit-codegen`, a tool that generates type-safe C++ bindings from WIT files.

### Build wit-codegen

From the component-model-cpp repository root:

```bash
cmake --preset linux-ninja-Debug
cmake --build --preset linux-ninja-Debug --target wit-codegen
```

### Generate Host Bindings

```bash
# Generate bindings into host/generated/
cd calculator-app/host
mkdir -p generated
/path/to/component-model-cpp/build/tools/wit-codegen/wit-codegen \
    ../calculator.wit

# This creates in the current directory:
# - calculator.hpp         (function declarations)
# - calculator_wamr.hpp    (WAMR integration helpers)  
# - calculator_wamr.cpp    (WAMR symbol registration)
```

Move the generated files to your generated directory:

```bash
mv calculator*.hpp calculator*.cpp generated/
```

### Understanding Generated Files

**calculator.hpp**: Type-safe function declarations
```cpp
namespace host {
    namespace operations {
        cmcpp::float64_t multiply(cmcpp::float64_t a, cmcpp::float64_t b);
        cmcpp::result_t<cmcpp::float64_t, cmcpp::string_t> divide(
            cmcpp::float64_t a, cmcpp::float64_t b);
    }
}

namespace guest {
    namespace compute {
        using circle_area_t = cmcpp::float64_t(cmcpp::float64_t);
        using compound_interest_t = cmcpp::float64_t(cmcpp::float64_t, cmcpp::float64_t, uint32_t);
        // ... and struct definitions
    }
}
```

**calculator_wamr.hpp/cpp**: WAMR runtime integration
- Automatic type marshaling between C++ and WebAssembly
- Symbol registration arrays
- Helper macros for host/guest function wrapping

## Step 4: Implement Host Functions

Create `host/host_impl.cpp`:

```cpp
#include "generated/calculator.hpp"
#include <iostream>
#include <limits>
#include <cmath>

// Implementations in the 'host' namespace are called by the guest

namespace host {
namespace operations {

cmcpp::float64_t multiply(cmcpp::float64_t a, cmcpp::float64_t b)
{
    double result = a * b;
    std::cout << "[HOST] multiply(" << a << ", " << b << ") = " << result << std::endl;
    return result;
}

cmcpp::result_t<cmcpp::float64_t, cmcpp::string_t> divide(
    cmcpp::float64_t a, 
    cmcpp::float64_t b)
{
    std::cout << "[HOST] divide(" << a << ", " << b << ")";
    
    if (b == 0.0 || std::isnan(b) || std::isnan(a)) {
        std::cout << " = ERROR: Invalid operands" << std::endl;
        return cmcpp::string_t("Division by zero or NaN operand");
    }
    
    double result = a / b;
    
    if (std::isinf(result)) {
        std::cout << " = ERROR: Result overflow" << std::endl;
        return cmcpp::string_t("Result overflow");
    }
    
    std::cout << " = " << result << std::endl;
    return result;
}

} // namespace operations
} // namespace host
```

## Step 5: Create the WAMR Host Application

Create `host/main.cpp`:

```cpp
#include "generated/calculator_wamr.hpp"  // Includes calculator.hpp and wamr helpers
#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>

// Load WebAssembly binary into memory
std::vector<uint8_t> read_wasm_file(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open: " + path.string());
    }
    
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        throw std::runtime_error("Failed to read: " + path.string());
    }
    
    return buffer;
}

int main(int argc, char** argv)
{
    std::cout << "Calculator WAMR Host Application\n";
    std::cout << "=================================\n\n";
    
    // Initialize WAMR runtime
    RuntimeInitArgs init_args;
    std::memset(&init_args, 0, sizeof(RuntimeInitArgs));
    init_args.mem_alloc_type = Alloc_With_System_Allocator;
    
    if (!wasm_runtime_full_init(&init_args)) {
        std::cerr << "Failed to initialize WAMR runtime" << std::endl;
        return 1;
    }
    
    try {
        // Load WebAssembly module
        std::filesystem::path wasm_path = "../guest/build/calculator_component.wasm";
        auto wasm_bytes = read_wasm_file(wasm_path);
        
        std::cout << "Loaded WASM module: " << wasm_bytes.size() << " bytes\n\n";
        
        // Parse and load module
        char error_buf[128];
        wasm_module_t module = wasm_runtime_load(
            wasm_bytes.data(), 
            wasm_bytes.size(),
            error_buf, 
            sizeof(error_buf)
        );
        
        if (!module) {
            throw std::runtime_error(std::string("Failed to load module: ") + error_buf);
        }
        
        // Instantiate module with host functions
        wasm_module_inst_t instance = wasm_runtime_instantiate(
            module,
            64 * 1024,  // stack size
            64 * 1024,  // heap size
            error_buf,
            sizeof(error_buf)
        );
        
        if (!instance) {
            wasm_runtime_unload(module);
            throw std::runtime_error(std::string("Failed to instantiate: ") + error_buf);
        }
        
        // Register host functions (from generated calculator_wamr.cpp)
        if (!wasm_runtime_register_natives(
                "math:calculator/operations",
                calculator_host_operations,
                calculator_host_operations_count)) {
            std::cerr << "Warning: Failed to register host functions" << std::endl;
        }
        
        // Create execution environment
        wasm_exec_env_t exec_env = wasm_runtime_create_exec_env(instance, 64 * 1024);
        if (!exec_env) {
            wasm_runtime_deinstantiate(instance);
            wasm_runtime_unload(module);
            throw std::runtime_error("Failed to create execution environment");
        }
        
        std::cout << "=== Calling Guest Functions ===\n\n";
        
        // Test 1: Circle area calculation
        {
            auto circle_area = cmcpp::guest_function<guest::compute::circle_area_t>(
                instance, 
                "math:calculator/compute", 
                "circle-area"
            );
            
            double radius = 5.0;
            double area = circle_area(radius);
            std::cout << "Circle area (radius=" << radius << "): " << area << std::endl;
        }
        
        // Test 2: Compound interest calculation
        {
            auto compound_interest = cmcpp::guest_function<guest::compute::compound_interest_t>(
                instance,
                "math:calculator/compute",
                "compound-interest"
            );
            
            double principal = 1000.0;
            double rate = 0.05;
            uint32_t years = 10;
            double final_amount = compound_interest(principal, rate, years);
            
            std::cout << "Compound interest ($" << principal 
                      << " @ " << (rate * 100) << "% for " << years 
                      << " years): $" << final_amount << std::endl;
        }
        
        // Test 3: Batch statistics
        {
            auto batch_stats = cmcpp::guest_function<guest::compute::batch_stats_t>(
                instance,
                "math:calculator/compute",
                "batch-stats"
            );
            
            cmcpp::list_t<cmcpp::float64_t> numbers = {10.5, 20.3, 15.7, 30.2, 5.1};
            auto stats = batch_stats(numbers);
            
            std::cout << "\nBatch statistics:\n";
            std::cout << "  Count: " << stats.count << "\n";
            std::cout << "  Sum: " << stats.sum << "\n";
            std::cout << "  Average: " << stats.average << "\n";
            std::cout << "  Min: " << stats.min << "\n";
            std::cout << "  Max: " << stats.max << std::endl;
        }
        
        // Cleanup
        wasm_runtime_destroy_exec_env(exec_env);
        wasm_runtime_deinstantiate(instance);
        wasm_runtime_unload(module);
        
        std::cout << "\n=== Success ===\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        wasm_runtime_destroy();
        return 1;
    }
    
    wasm_runtime_destroy();
    return 0;
}
```

## Step 6: Build the Host Application

Create `host/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.10)
project(calculator-host LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find component-model-cpp
find_package(cmcpp REQUIRED)

# Find WAMR (assuming vcpkg installation)
find_path(WAMR_INCLUDE_DIRS NAMES wasm_export.h REQUIRED)
find_library(IWASM_LIB NAMES iwasm libiwasm REQUIRED)

# Build the host executable
add_executable(calculator-host
    main.cpp
    host_impl.cpp
    generated/calculator_wamr.cpp
)

target_include_directories(calculator-host PRIVATE
    ${WAMR_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/generated
)

target_link_libraries(calculator-host PRIVATE
    cmcpp::cmcpp
    ${IWASM_LIB}
)
```

### Build and Run

```bash
cd host
cmake -DCMAKE_PREFIX_PATH=/path/to/component-model-cpp/install \
      -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
      -B build
cmake --build build

# Run
./build/calculator-host
```

Expected output:

```
Calculator WAMR Host Application
=================================

Loaded WASM module: 45678 bytes

=== Calling Guest Functions ===

[HOST] multiply(5, 5) = 25
[HOST] multiply(3.14159, 25) = 78.5398
Circle area (radius=5): 78.5398

[HOST] multiply(1000, 1.05) = 1050
[HOST] multiply(1050, 1.05) = 1102.5
... (more multiplications)
Compound interest ($1000 @ 5% for 10 years): $1628.89

Batch statistics:
  Count: 5
  Sum: 81.8
  Average: 16.36
  Min: 5.1
  Max: 30.2

=== Success ===
```

## Summary

You've successfully created a complete WebAssembly Component Model application! Here's what you learned:

1. **WIT Interface Design**: Defined bidirectional interfaces with rich types
2. **Guest Development**: Built a WebAssembly component using wit-bindgen
3. **Host Binding Generation**: Used wit-codegen for type-safe C++ bindings
4. **Host Implementation**: Implemented host functions called by the guest
5. **Runtime Integration**: Integrated WAMR to execute WebAssembly components
6. **Type Safety**: Leveraged cmcpp type system for automatic marshaling

## Next Steps

- Explore advanced types: variants, enums, flags, resources
- Add error handling with `result<T, E>` types
- Implement streaming interfaces for large data processing
- Try other WebAssembly runtimes (WasmTime, WasmEdge)
- Read the [API Examples](api-examples.md) for more patterns

## Resources

- [Component Model Specification](https://github.com/WebAssembly/component-model)
- [WIT Language Guide](https://github.com/WebAssembly/component-model/blob/main/design/mvp/WIT.md)
- [WAMR Documentation](https://github.com/bytecodealliance/wasm-micro-runtime)
- [Sample Code](../samples/) in this repository
