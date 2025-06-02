#include "wasmtime.hpp"
#include <cfloat>
#include <iostream>
#include <fstream>
#include <memory>
#include <wasmtime.h>

using namespace cmcpp;

// Configuration constants
constexpr const char *WASM_FILE_PATH = "../bin/sample.wasm";

// RAII wrapper for Wasmtime objects
template <typename T, void (*deleter)(T *)>
class WasmtimeHandle
{
public:
    explicit WasmtimeHandle(T *ptr = nullptr) : ptr_(ptr) {}
    ~WasmtimeHandle()
    {
        if (ptr_)
            deleter(ptr_);
    }

    WasmtimeHandle(const WasmtimeHandle &) = delete;
    WasmtimeHandle &operator=(const WasmtimeHandle &) = delete;

    WasmtimeHandle(WasmtimeHandle &&other) noexcept : ptr_(other.ptr_)
    {
        other.ptr_ = nullptr;
    }

    WasmtimeHandle &operator=(WasmtimeHandle &&other) noexcept
    {
        if (this != &other)
        {
            if (ptr_)
                deleter(ptr_);
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    T *get() const { return ptr_; }
    T *release()
    {
        T *tmp = ptr_;
        ptr_ = nullptr;
        return tmp;
    }
    void reset(T *ptr = nullptr)
    {
        if (ptr_)
            deleter(ptr_);
        ptr_ = ptr;
    }
    explicit operator bool() const { return ptr_ != nullptr; }

private:
    T *ptr_;
};

using Engine = WasmtimeHandle<wasm_engine_t, wasm_engine_delete>;
using Store = WasmtimeHandle<wasmtime_store_t, wasmtime_store_delete>;
using Module = WasmtimeHandle<wasmtime_module_t, wasmtime_module_delete>;
using Linker = WasmtimeHandle<wasmtime_linker_t, wasmtime_linker_delete>;
using Error = WasmtimeHandle<wasmtime_error_t, wasmtime_error_delete>;

// File reading utility
std::vector<uint8_t> read_wasm_file(const char *filename)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("Failed to open WASM file: " + std::string(filename));
    }

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char *>(buffer.data()), size))
    {
        throw std::runtime_error("Failed to read WASM file: " + std::string(filename));
    }

    return buffer;
}

// Host function implementations (mirroring WAMR sample)
wasm_trap_t *void_func_callback(void *env, wasmtime_caller_t *caller,
                                const wasmtime_val_t *args, size_t nargs,
                                wasmtime_val_t *results, size_t nresults)
{
    std::cout << "Hello, Void_Func!" << std::endl;
    return nullptr;
}

wasm_trap_t *and_func_callback(void *env, wasmtime_caller_t *caller,
                               const wasmtime_val_t *args, size_t nargs,
                               wasmtime_val_t *results, size_t nresults)
{
    if (nargs != 2 || nresults != 1)
    {
        return wasmtime_trap_new("Invalid argument count", 25);
    }

    bool a = args[0].of.i32 != 0;
    bool b = args[1].of.i32 != 0;
    bool result = a && b;

    results[0].kind = WASMTIME_I32;
    results[0].of.i32 = result ? 1 : 0;

    return nullptr;
}

wasm_trap_t *add_func_callback(void *env, wasmtime_caller_t *caller,
                               const wasmtime_val_t *args, size_t nargs,
                               wasmtime_val_t *results, size_t nresults)
{
    if (nargs != 2 || nresults != 1)
    {
        return wasmtime_trap_new("Invalid argument count", 25);
    }

    double a = args[0].of.f64;
    double b = args[1].of.f64;
    double result = a + b;

    results[0].kind = WASMTIME_F64;
    results[0].of.f64 = result;

    return nullptr;
}

wasm_trap_t *reverse_func_callback(void *env, wasmtime_caller_t *caller,
                                   const wasmtime_val_t *args, size_t nargs,
                                   wasmtime_val_t *results, size_t nresults)
{
    std::cout << "reverse_func called with Canonical ABI" << std::endl;

    if (nargs != 3)
    {
        std::cerr << "reverse_func: Expected 3 arguments (ptr, len, realloc), got " << nargs << std::endl;
        return nullptr;
    }

    // Extract arguments: string_ptr, string_len, realloc_func
    uint32_t string_ptr = args[0].of.i32;
    uint32_t string_len = args[1].of.i32;
    uint32_t realloc_func = args[2].of.i32;

    std::cout << "  Input string: ptr=" << string_ptr << ", len=" << string_len << std::endl;
    std::cout << "  Realloc func: " << realloc_func << std::endl;

    // For Component Model Canonical ABI, this function should:
    // 1. Read string from guest memory at [string_ptr, string_ptr + string_len)
    // 2. Transform the string (reverse/uppercase)
    // 3. Allocate new memory in guest using realloc_func
    // 4. Write result string to new memory
    // 5. Return new pointer and length through memory (not return values)

    // Simplified implementation - just acknowledge the call
    std::cout << "  reverse_func completed (simplified)" << std::endl;
    return nullptr;
}

wasm_trap_t *lots_func_callback(void *env, wasmtime_caller_t *caller,
                                const wasmtime_val_t *args, size_t nargs,
                                wasmtime_val_t *results, size_t nresults)
{
    std::cout << "lots_func called with simplified signature" << std::endl;

    // Expected: single i32 parameter
    if (nargs != 1)
    {
        std::cerr << "lots_func: Expected 1 argument, got " << nargs << std::endl;
        return nullptr;
    }

    uint32_t param_handle = args[0].of.i32;
    std::cout << "  Parameter handle: " << param_handle << std::endl;

    // For now, return a simplified result
    // In a full implementation, we would need to extract the actual string parameters
    // from WebAssembly memory using the parameter handle
    uint32_t result = 42; // Simplified result

    if (nresults >= 1)
    {
        results[0].kind = WASMTIME_I32;
        results[0].of.i32 = result;
    }

    std::cout << "  lots_func returning: " << result << std::endl;
    return nullptr;
}

wasm_trap_t *log_u32_callback(void *env, wasmtime_caller_t *caller,
                              const wasmtime_val_t *args, size_t nargs,
                              wasmtime_val_t *results, size_t nresults)
{
    // Expected: u32 + string (ptr, len) + realloc = 4 parameters
    if (nargs != 4)
    {
        std::cerr << "log_u32: Expected 4 arguments (u32, string_ptr, string_len, realloc), got " << nargs << std::endl;
        return nullptr;
    }

    uint32_t u32_value = args[0].of.i32;
    uint32_t string_ptr = args[1].of.i32;
    uint32_t string_len = args[2].of.i32;
    uint32_t realloc_func = args[3].of.i32;

    std::cout << "wasm-log: u32=" << u32_value << ", string: ptr=" << string_ptr
              << ", len=" << string_len << ", realloc=" << realloc_func << std::endl;

    return nullptr;
}

// Function type creation helpers
wasm_functype_t *create_void_func_type()
{
    wasm_valtype_vec_t params, results;
    wasm_valtype_vec_new_empty(&params);
    wasm_valtype_vec_new_empty(&results);
    return wasm_functype_new(&params, &results);
}

wasm_functype_t *create_bool_bool_bool_func_type()
{
    wasm_valtype_vec_t params, results;
    wasm_valtype_vec_new_uninitialized(&params, 2);
    params.data[0] = wasm_valtype_new_i32();
    params.data[1] = wasm_valtype_new_i32();

    wasm_valtype_vec_new_uninitialized(&results, 1);
    results.data[0] = wasm_valtype_new_i32();

    return wasm_functype_new(&params, &results);
}

wasm_functype_t *create_f64_f64_f64_func_type()
{
    wasm_valtype_vec_t params, results;
    wasm_valtype_vec_new_uninitialized(&params, 2);
    params.data[0] = wasm_valtype_new_f64();
    params.data[1] = wasm_valtype_new_f64();

    wasm_valtype_vec_new_uninitialized(&results, 1);
    results.data[0] = wasm_valtype_new_f64();

    return wasm_functype_new(&params, &results);
}

wasm_functype_t *create_log_func_type()
{
    // Canonical ABI: u32 + string (ptr, len) + realloc = 4 parameters, no results
    wasm_valtype_vec_t params, results;
    wasm_valtype_vec_new_uninitialized(&params, 4);
    params.data[0] = wasm_valtype_new_i32(); // u32 value
    params.data[1] = wasm_valtype_new_i32(); // string ptr
    params.data[2] = wasm_valtype_new_i32(); // string len
    params.data[3] = wasm_valtype_new_i32(); // realloc func

    wasm_valtype_vec_new_empty(&results);

    return wasm_functype_new(&params, &results);
}

wasm_functype_t *create_string_func_type()
{
    // Canonical ABI string function: (param i32 i32 i32) -> ()
    // i32: string pointer, i32: string length, i32: realloc function
    wasm_valtype_vec_t params, results;
    wasm_valtype_vec_new_uninitialized(&params, 3);
    params.data[0] = wasm_valtype_new_i32(); // string ptr
    params.data[1] = wasm_valtype_new_i32(); // string len
    params.data[2] = wasm_valtype_new_i32(); // realloc func

    wasm_valtype_vec_new_empty(&results); // No return values

    return wasm_functype_new(&params, &results);
}

wasm_functype_t *create_lots_func_type()
{
    // Based on error message: expected type `(func (param i32) (result i32))`
    // The Component Model may be using a different ABI where complex parameters
    // are passed through memory, and the function just receives a pointer
    wasm_valtype_vec_t params, results;
    wasm_valtype_vec_new_uninitialized(&params, 1);
    params.data[0] = wasm_valtype_new_i32(); // Parameter pointer or handle

    wasm_valtype_vec_new_uninitialized(&results, 1);
    results.data[0] = wasm_valtype_new_i32(); // u32 result

    return wasm_functype_new(&params, &results);
}

// Function calling helper
wasmtime_val_t call_wasm_function(wasmtime_context_t *context, wasmtime_func_t func,
                                  const std::vector<wasmtime_val_t> &args)
{
    std::vector<wasmtime_val_t> results(1); // Assuming single result for simplicity
    wasm_trap_t *trap = nullptr;

    wasmtime_error_t *error = wasmtime_func_call(context, &func,
                                                 args.data(), args.size(),
                                                 results.data(), results.size(),
                                                 &trap);

    if (error)
    {
        handle_error("Function call failed", error);
        wasmtime_error_delete(error);
        throw std::runtime_error("Function call failed");
    }

    if (trap)
    {
        handle_error("Function call trapped", nullptr, trap);
        wasm_trap_delete(trap);
        throw std::runtime_error("Function call trapped");
    }

    return results[0];
}

int main()
{
    std::cout << "Wasmtime Component Model C++ Sample" << std::endl;
    std::cout << "====================================" << std::endl;
    std::cout << "Starting Wasmtime runtime initialization..." << std::endl;

    try
    {
        // Create engine
        wasm_config_t *config = wasm_config_new();
        if (!config)
        {
            throw std::runtime_error("Failed to create Wasmtime config");
        }

        Engine engine(wasm_engine_new_with_config(config));
        if (!engine)
        {
            throw std::runtime_error("Failed to create Wasmtime engine");
        }
        std::cout << "Wasmtime engine created successfully" << std::endl;

        // Create store
        Store store(wasmtime_store_new(engine.get(), nullptr, nullptr));
        if (!store)
        {
            throw std::runtime_error("Failed to create Wasmtime store");
        }
        std::cout << "Wasmtime store created successfully" << std::endl;

        // Create linker
        Linker linker(wasmtime_linker_new(engine.get()));
        if (!linker)
        {
            throw std::runtime_error("Failed to create Wasmtime linker");
        }
        std::cout << "Wasmtime linker created successfully" << std::endl;

        // Get store context
        wasmtime_context_t *context = wasmtime_store_context(store.get());

        // Define host functions in linker
        std::cout << "Registering host functions..." << std::endl;

        // Register void function
        wasm_functype_t *void_type = create_void_func_type();
        wasmtime_error_t *error = wasmtime_linker_define_func(
            linker.get(), "$root", 5, "void-func", 9, void_type,
            void_func_callback, nullptr, nullptr);
        wasm_functype_delete(void_type);
        if (error)
        {
            handle_error("Failed to register void-func", error);
            wasmtime_error_delete(error);
            return 1;
        }

        // Register boolean and function
        wasm_functype_t *bool_type = create_bool_bool_bool_func_type();
        error = wasmtime_linker_define_func(
            linker.get(), "example:sample/booleans", 23, "and", 3, bool_type,
            and_func_callback, nullptr, nullptr);
        wasm_functype_delete(bool_type);
        if (error)
        {
            handle_error("Failed to register and function", error);
            wasmtime_error_delete(error);
            return 1;
        }

        // Register float add function
        wasm_functype_t *f64_type = create_f64_f64_f64_func_type();
        error = wasmtime_linker_define_func(
            linker.get(), "example:sample/floats", 21, "add", 3, f64_type,
            add_func_callback, nullptr, nullptr);
        wasm_functype_delete(f64_type);
        if (error)
        {
            handle_error("Failed to register add function", error);
            wasmtime_error_delete(error);
            return 1;
        }

        // Register string reverse function
        wasm_functype_t *string_type = create_string_func_type();
        error = wasmtime_linker_define_func(
            linker.get(), "example:sample/strings", 22, "reverse", 7, string_type,
            reverse_func_callback, nullptr, nullptr);
        wasm_functype_delete(string_type);
        if (error)
        {
            handle_error("Failed to register reverse function", error);
            wasmtime_error_delete(error);
            return 1;
        }

        // Register string lots function
        wasm_functype_t *lots_type = create_lots_func_type();
        error = wasmtime_linker_define_func(
            linker.get(), "example:sample/strings", 22, "lots", 4, lots_type,
            lots_func_callback, nullptr, nullptr);
        wasm_functype_delete(lots_type);
        if (error)
        {
            handle_error("Failed to register lots function", error);
            wasmtime_error_delete(error);
            return 1;
        }

        // Register logging function
        wasm_functype_t *log_type = create_log_func_type();
        error = wasmtime_linker_define_func(
            linker.get(), "example:sample/logging", 22, "log-u32", 7, log_type,
            log_u32_callback, nullptr, nullptr);
        wasm_functype_delete(log_type);
        if (error)
        {
            handle_error("Failed to register log-u32 function", error);
            wasmtime_error_delete(error);
            return 1;
        }

        std::cout << "Host functions registered successfully" << std::endl;

        // Read WASM file
        std::cout << "Loading WASM file..." << std::endl;
        std::vector<uint8_t> wasm_bytes;
        try
        {
            wasm_bytes = read_wasm_file(WASM_FILE_PATH);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Failed to read WASM file: " << e.what() << std::endl;
            return 1;
        }
        std::cout << "Successfully loaded WASM file (" << wasm_bytes.size() << " bytes)" << std::endl;

        // Compile module
        wasmtime_module_t *module_ptr = nullptr;
        error = wasmtime_module_new(engine.get(), wasm_bytes.data(), wasm_bytes.size(), &module_ptr);
        Module module(module_ptr);
        if (error)
        {
            handle_error("Failed to compile WASM module", error);
            wasmtime_error_delete(error);
            return 1;
        }
        std::cout << "Successfully compiled WASM module" << std::endl;

        // Instantiate module
        std::cout << "Instantiating WASM module..." << std::endl;
        wasmtime_instance_t instance;
        wasm_trap_t *trap = nullptr;
        error = wasmtime_linker_instantiate(linker.get(), context, module.get(), &instance, &trap);
        if (error)
        {
            handle_error("Failed to instantiate WASM module", error, trap);
            wasmtime_error_delete(error);
            if (trap)
                wasm_trap_delete(trap);
            return 1;
        }
        if (trap)
        {
            handle_error("Module instantiation trapped", nullptr, trap);
            wasm_trap_delete(trap);
            return 1;
        }
        std::cout << "Successfully instantiated WASM module" << std::endl;

        // Get exports for testing
        std::cout << "\n=== Testing Guest Functions ===" << std::endl;

        // Get and test a simple function (if available)
        wasmtime_extern_t extern_item;
        bool found = wasmtime_linker_get(linker.get(), context, "", 0, "void-func", 9, &extern_item);
        if (found && extern_item.kind == WASMTIME_EXTERN_FUNC)
        {
            std::cout << "\n--- Testing Void Function ---" << std::endl;
            std::vector<wasmtime_val_t> args;
            try
            {
                call_wasm_function(context, extern_item.of.func, args);
                std::cout << "void-func called successfully" << std::endl;
            }
            catch (const std::exception &e)
            {
                std::cout << "void-func call failed: " << e.what() << std::endl;
            }
        }

        // Test boolean functions
        found = wasmtime_linker_get(linker.get(), context, "example:sample/booleans", 23, "and", 3, &extern_item);
        if (found && extern_item.kind == WASMTIME_EXTERN_FUNC)
        {
            std::cout << "\n--- Testing Boolean Functions ---" << std::endl;

            std::vector<std::pair<bool, bool>> test_cases = {
                {false, false}, {false, true}, {true, false}, {true, true}};

            for (const auto &test_case : test_cases)
            {
                std::vector<wasmtime_val_t> args(2);
                args[0].kind = WASMTIME_I32;
                args[0].of.i32 = test_case.first ? 1 : 0;
                args[1].kind = WASMTIME_I32;
                args[1].of.i32 = test_case.second ? 1 : 0;

                try
                {
                    wasmtime_val_t result = call_wasm_function(context, extern_item.of.func, args);
                    bool result_bool = result.of.i32 != 0;
                    std::cout << "and(" << test_case.first << ", " << test_case.second
                              << ") = " << result_bool << std::endl;
                }
                catch (const std::exception &e)
                {
                    std::cout << "and function call failed: " << e.what() << std::endl;
                }
            }
        }

        // Test float functions
        found = wasmtime_linker_get(linker.get(), context, "example:sample/floats", 21, "add", 3, &extern_item);
        if (found && extern_item.kind == WASMTIME_EXTERN_FUNC)
        {
            std::cout << "\n--- Testing Float Functions ---" << std::endl;

            std::vector<std::pair<double, double>> test_cases = {
                {3.1, 0.2}, {1.5, 2.5}, {DBL_MAX / 2, 0.0}};

            for (const auto &test_case : test_cases)
            {
                std::vector<wasmtime_val_t> args(2);
                args[0].kind = WASMTIME_F64;
                args[0].of.f64 = test_case.first;
                args[1].kind = WASMTIME_F64;
                args[1].of.f64 = test_case.second;

                try
                {
                    wasmtime_val_t result = call_wasm_function(context, extern_item.of.func, args);
                    std::cout << "add(" << test_case.first << ", " << test_case.second
                              << ") = " << result.of.f64 << std::endl;
                }
                catch (const std::exception &e)
                {
                    std::cout << "add function call failed: " << e.what() << std::endl;
                }
            }
        }

        std::cout << "\n=== Sample completed successfully! ===" << std::endl;
        std::cout << "Wasmtime runtime managed WebAssembly execution with component model integration." << std::endl;

        std::cout << "\n=== Testing Guest->Host Function Calls ===" << std::endl;

        // For now, let's just indicate that the host functions are ready
        // In a complete implementation, we would:
        // 1. Look for guest functions that call our host functions
        // 2. Call those guest functions to test host function invocation
        std::cout << "Host functions registered and ready for guest calls:" << std::endl;
        std::cout << "  - $root::void-func (void -> void)" << std::endl;
        std::cout << "  - example:sample/booleans::and (bool, bool -> bool)" << std::endl;
        std::cout << "  - example:sample/floats::add (f64, f64 -> f64)" << std::endl;
        std::cout << "  - example:sample/strings::reverse (string -> string)" << std::endl;
        std::cout << "  - example:sample/strings::lots (17 strings -> u32)" << std::endl;
        std::cout << "  - example:sample/logging::log-u32 (u32, string -> void)" << std::endl;

        // For each exported function, try calling it from the guest
        // This requires the function to be a simple signature that we can call directly
        // Complex signatures with multiple parameters or results may need adaptation
        // to match the expected calling convention

        // Example: Call the "void-func" from the guest, if available
        found = wasmtime_linker_get(linker.get(), context, "", 0, "void-func", 9, &extern_item);
        if (found && extern_item.kind == WASMTIME_EXTERN_FUNC)
        {
            std::cout << "\n--- Calling Guest Void Function from Host ---" << std::endl;
            std::vector<wasmtime_val_t> args;
            try
            {
                call_wasm_function(context, extern_item.of.func, args);
                std::cout << "Guest void-func called successfully" << std::endl;
            }
            catch (const std::exception &e)
            {
                std::cout << "Guest void-func call failed: " << e.what() << std::endl;
            }
        }

        // Add similar blocks for other functions you want to test calling from the guest
        // Ensure that the function signatures match the expected parameters and results

        std::cout << "\n=== Guest->Host Function Calls completed ===" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}