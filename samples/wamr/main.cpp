#include "wamr.hpp"
#include <algorithm>
#include <cfloat>
#include <iostream>
#include <tuple>

using namespace cmcpp;

// Configuration constants
constexpr uint32_t DEFAULT_STACK_SIZE = 8192;
constexpr uint32_t DEFAULT_HEAP_SIZE = 8192;
constexpr const char *WASM_FILE_PATH = "../bin/sample.wasm";

char *read_wasm_binary_to_buffer(const char *filename, uint32_t *size)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return nullptr;
    }

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = new char[*size];
    if (fread(buffer, 1, *size, file) != *size)
    {
        std::cerr << "Failed to read file: " << filename << std::endl;
        delete[] buffer;
        fclose(file);
        return nullptr;
    }

    fclose(file);
    return buffer;
}

void_t void_func()
{
    std::cout << "Hello, Void_Func!" << std::endl;
}
NativeSymbol root_symbol[] = {
    host_function("void-func", void_func),
};

cmcpp::bool_t and_func(cmcpp::bool_t a, cmcpp::bool_t b)
{
    return a && b;
}
NativeSymbol booleans_symbol[] = {
    host_function("and", and_func),
};

float64_t add(float64_t a, float64_t b)
{
    return a + b;
}
NativeSymbol floats_symbol[] = {
    host_function("add", add),
};

string_t reverse(const string_t &a)
{
    std::string result = a;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}
uint32_t lots(const string_t &p1, const string_t &p2, const string_t &p3, const string_t &p4, const string_t &p5, const string_t &p6, const string_t &p7, const string_t &p8, const string_t &p9, const string_t &p10, const string_t &p11, const string_t &p12, const string_t &p13, const string_t &p14, const string_t &p15, const string_t &p16, const string_t &p17)
{
    return p1.length() + p2.length() + p3.length() + p4.length() + p5.length() + p6.length() + p7.length() + p8.length() + p9.length() + p10.length() + p11.length() + p12.length() + p13.length() + p14.length() + p15.length() + p16.length() + p17.length();
}
NativeSymbol strings_symbol[] = {
    host_function("reverse", reverse),
    host_function("lots", lots),
};

void_t log_u32(uint32_t a, string_t b)
{
    std::cout << "wasm-log:  " << b << a << std::endl;
}
NativeSymbol logging_symbol[] = {
    host_function("log-u32", log_u32),
};

int main()
{
    std::cout << "WAMR Component Model C++ Sample" << std::endl;
    std::cout << "===============================" << std::endl;
    std::cout << "Starting WAMR runtime initialization..." << std::endl;

    char *buffer, error_buf[128];
    wasm_module_t module;
    wasm_module_inst_t module_inst;
    wasm_function_inst_t cabi_realloc;
    wasm_exec_env_t exec_env;
    uint32_t size, stack_size = DEFAULT_STACK_SIZE, heap_size = DEFAULT_HEAP_SIZE;

    /* initialize the wasm runtime by default configurations */
    wasm_runtime_init();
    std::cout << "WAMR runtime initialized successfully" << std::endl;

    /* read WASM file into a memory buffer */
    buffer = read_wasm_binary_to_buffer(WASM_FILE_PATH, &size);
    if (!buffer)
    {
        std::cerr << "Failed to read WASM file" << std::endl;
        return 1;
    }
    std::cout << "Successfully loaded WASM file (" << size << " bytes)" << std::endl;

    // /* add line below if we want to export native functions to WASM app */
    bool success = wasm_runtime_register_natives_raw("$root", root_symbol, sizeof(root_symbol) / sizeof(NativeSymbol));
    if (!success)
    {
        std::cerr << "Failed to register $root natives" << std::endl;
        return 1;
    }
    success = wasm_runtime_register_natives_raw("example:sample/booleans", booleans_symbol, sizeof(booleans_symbol) / sizeof(NativeSymbol));
    if (!success)
    {
        std::cerr << "Failed to register booleans natives" << std::endl;
        return 1;
    }
    success = wasm_runtime_register_natives_raw("example:sample/floats", floats_symbol, sizeof(floats_symbol) / sizeof(NativeSymbol));
    if (!success)
    {
        std::cerr << "Failed to register floats natives" << std::endl;
        return 1;
    }
    success = wasm_runtime_register_natives_raw("example:sample/strings", strings_symbol, sizeof(strings_symbol) / sizeof(NativeSymbol));
    if (!success)
    {
        std::cerr << "Failed to register strings natives" << std::endl;
        return 1;
    }
    success = wasm_runtime_register_natives_raw("example:sample/logging", logging_symbol, sizeof(logging_symbol) / sizeof(NativeSymbol));
    if (!success)
    {
        std::cerr << "Failed to register logging natives" << std::endl;
        return 1;
    }

    // /* parse the WASM file from buffer and create a WASM module */
    module = wasm_runtime_load((uint8_t *)buffer, size, error_buf, sizeof(error_buf));
    if (!module)
    {
        std::cerr << "Failed to load WASM module: " << error_buf << std::endl;
        delete[] buffer;
        wasm_runtime_destroy();
        return 1;
    }
    std::cout << "Successfully loaded WASM module" << std::endl;

    // /* create an instance of the WASM module (WASM linear memory is ready) */
    module_inst = wasm_runtime_instantiate(module, stack_size, heap_size, error_buf, sizeof(error_buf));
    if (!module_inst)
    {
        std::cerr << "Failed to instantiate WASM module: " << error_buf << std::endl;
        wasm_runtime_unload(module);
        delete[] buffer;
        wasm_runtime_destroy();
        return 1;
    }
    std::cout << "Successfully instantiated WASM module" << std::endl;

    wasm_memory_inst_t memory = wasm_runtime_lookup_memory(module_inst, "memory");
    if (!memory)
    {
        std::cerr << "Failed to lookup memory instance" << std::endl;
        wasm_runtime_deinstantiate(module_inst);
        wasm_runtime_unload(module);
        delete[] buffer;
        wasm_runtime_destroy();
        return 1;
    }

    cabi_realloc = wasm_runtime_lookup_function(module_inst, "cabi_realloc");
    if (!cabi_realloc)
    {
        std::cerr << "Failed to lookup cabi_realloc function" << std::endl;
        wasm_runtime_deinstantiate(module_inst);
        wasm_runtime_unload(module);
        delete[] buffer;
        wasm_runtime_destroy();
        return 1;
    }

    // auto cx = createCallContext(&heap, Encoding::Utf8);

    exec_env = wasm_runtime_create_exec_env(module_inst, stack_size);

    GuestRealloc realloc = [exec_env, cabi_realloc](int original_ptr, int original_size, int alignment, int new_size) -> int
    {
        uint32_t argv[4];
        argv[0] = original_ptr;
        argv[1] = original_size;
        argv[2] = alignment;
        argv[3] = new_size;
        wasm_runtime_call_wasm(exec_env, cabi_realloc, 4, argv);
        return argv[0];
    };

    uint8_t *mem_start_addr = (uint8_t *)wasm_memory_get_base_address(memory);
    uint8_t *mem_end_addr = NULL;
    wasm_runtime_get_native_addr_range(module_inst, mem_start_addr, NULL, &mem_end_addr);
    LiftLowerOptions opts(Encoding::Utf8, std::span<uint8_t>(mem_start_addr, mem_end_addr - mem_start_addr), realloc);

    LiftLowerContext liftLowerContext(trap, convert, opts);

    std::cout << "\n=== Testing Guest Functions ===" << std::endl;

    std::cout << "\n--- String Functions ---" << std::endl;
    auto call_reverse = guest_function<string_t(string_t)>(module_inst, exec_env, liftLowerContext,
                                                           "example:sample/strings#reverse");
    auto call_reverse_result = call_reverse("Hello World!");
    std::cout << "call_reverse(\"Hello World!\"): " << call_reverse_result << std::endl;
    std::cout << "call_reverse(call_reverse(\"Hello World!\")): " << call_reverse(call_reverse_result) << std::endl;

    std::cout << "\n--- Variant Functions ---" << std::endl;
    auto variant_func = guest_function<variant_t<bool_t, uint32_t>(variant_t<bool_t, uint32_t>)>(module_inst, exec_env, liftLowerContext, "example:sample/variants#variant-func");
    std::cout << "variant_func((uint32_t)40)" << std::get<1>(variant_func((uint32_t)40)) << std::endl;
    std::cout << "variant_func((bool_t)true)" << std::get<0>(variant_func((bool_t) true)) << std::endl;
    std::cout << "variant_func((bool_t)false)" << std::get<0>(variant_func((bool_t) false)) << std::endl;

    std::cout << "\n--- Option Functions ---" << std::endl;
    auto option_func = guest_function<option_t<uint32_t>(option_t<uint32_t>)>(module_inst, exec_env, liftLowerContext, "option-func");
    std::cout << "option_func((uint32_t)40).has_value()" << option_func((uint32_t)40).has_value() << std::endl;
    std::cout << "option_func((uint32_t)40).value()" << option_func((uint32_t)40).value() << std::endl;
    std::cout << "option_func(std::nullopt).has_value()" << option_func(std::nullopt).has_value() << std::endl;

    std::cout << "\n--- Void Functions ---" << std::endl;
    auto void_func = guest_function<void()>(module_inst, exec_env, liftLowerContext, "void-func");
    void_func();

    std::cout << "\n--- Result Functions ---" << std::endl;
    auto ok_func = guest_function<result_t<uint32_t, string_t>(uint32_t, uint32_t)>(module_inst, exec_env, liftLowerContext, "ok-func");
    auto ok_func_result = ok_func(40, 2);
    std::cout << "ok_func result: " << std::get<uint32_t>(ok_func_result) << std::endl;

    auto err_func = guest_function<result_t<uint32_t, string_t>(uint32_t, uint32_t)>(module_inst, exec_env, liftLowerContext, "err-func");
    auto err_func_result = err_func(40, 2);
    std::cout << "err_func result: " << std::get<string_t>(err_func_result) << std::endl;

    std::cout << "\n--- Boolean Functions ---" << std::endl;
    auto call_and = guest_function<bool_t(bool_t, bool_t)>(module_inst, exec_env, liftLowerContext,
                                                           "example:sample/booleans#and");
    std::cout << "call_and(false, false): " << call_and(false, false) << std::endl;
    std::cout << "call_and(false, true): " << call_and(false, true) << std::endl;
    std::cout << "call_and(true, false): " << call_and(true, false) << std::endl;
    std::cout << "call_and(true, true): " << call_and(true, true) << std::endl;

    std::cout << "\n--- Float Functions ---" << std::endl;
    auto call_add = guest_function<float64_t(float64_t, float64_t)>(module_inst, exec_env, liftLowerContext,
                                                                    "example:sample/floats#add");
    std::cout << "call_add(3.1, 0.2): " << call_add(3.1, 0.2) << std::endl;
    std::cout << "call_add(1.5, 2.5): " << call_add(1.5, 2.5) << std::endl;
    std::cout << "call_add(DBL_MAX, 0.0): " << call_add(DBL_MAX / 2, 0.0) << std::endl;
    std::cout << "call_add(DBL_MAX / 2, DBL_MAX / 2): " << call_add(DBL_MAX / 2, DBL_MAX / 2) << std::endl;

    std::cout << "\n--- Complex String Functions ---" << std::endl;
    auto call_lots = guest_function<uint32_t(string_t, string_t, string_t, string_t, string_t, string_t, string_t, string_t, string_t, string_t, string_t, string_t, string_t, string_t, string_t, string_t, string_t)>(
        module_inst, exec_env, liftLowerContext,
        "example:sample/strings#lots");

    auto call_lots_result = call_lots(
        "p1", "p2", "p3", "p4", "p5", "p6", "p7", "p8",
        "p9", "p10", "p11", "p12", "p13", "p14", "p15", "p16", "p17");
    std::cout << "call_lots result: " << call_lots_result << std::endl;

    std::cout << "\n--- Tuple Functions ---" << std::endl;
    func_t<tuple_t<string_t, bool_t>(tuple_t<bool_t, string_t>)> host_reverse_tuple = [](tuple_t<bool_t, string_t> a) -> tuple_t<string_t, bool_t>
    {
        return {std::get<string_t>(a), std::get<bool_t>(a)};
    };
    // auto xxxx = host<tuple_t<string_t, bool_t>(tuple_t<bool_t, string_t>)>(liftLowerContext,
    //                                                                        "example:sample/tuples#reverse",
    //                                                                        host_reverse_tuple);
    auto call_reverse_tuple = guest_function<tuple_t<string_t, bool_t>(tuple_t<bool_t, string_t>)>(module_inst, exec_env, liftLowerContext,
                                                                                                   "example:sample/tuples#reverse");
    auto call_reverse_tuple_result = call_reverse_tuple({false, "Hello World!"});
    std::cout << "call_reverse_tuple({false, \"Hello World!\"}): " << std::get<0>(call_reverse_tuple_result) << ", " << std::get<1>(call_reverse_tuple_result) << std::endl;

    std::cout << "\n--- List Functions ---" << std::endl;
    auto call_list_filter = guest_function<list_t<string_t>(list_t<variant_t<bool_t, string_t>>)>(module_inst, exec_env, liftLowerContext, "example:sample/lists#filter-bool");
    auto call_list_filter_result = call_list_filter({{false}, {"Hello World!"}, {"Another String"}, {true}, {false}});
    std::cout << "call_list_filter result: " << call_list_filter_result.size() << std::endl;

    std::cout << "\n--- Enum Functions ---" << std::endl;
    enum e
    {
        a,
        b,
        c
    };

    auto enum_func = guest_function<enum_t<e>(enum_t<e>)>(module_inst, exec_env, liftLowerContext, "example:sample/enums#enum-func");
    std::cout << "enum_func(e::a): " << enum_func(e::a) << std::endl;
    std::cout << "enum_func(e::b): " << enum_func(e::b) << std::endl;
    std::cout << "enum_func(e::c): " << enum_func(e::c) << std::endl;

    std::cout << "\n=== Cleanup and Summary ===" << std::endl;
    // Clean up resources
    delete[] buffer;
    wasm_runtime_destroy_exec_env(exec_env);
    wasm_runtime_unregister_natives("$root", root_symbol);
    wasm_runtime_unregister_natives("example:sample/booleans", booleans_symbol);
    wasm_runtime_unregister_natives("example:sample/floats", floats_symbol);
    wasm_runtime_unregister_natives("example:sample/strings", strings_symbol);
    wasm_runtime_unregister_natives("example:sample/logging", logging_symbol);
    wasm_runtime_deinstantiate(module_inst);
    wasm_runtime_unload(module);
    wasm_runtime_destroy();

    std::cout << "Sample completed successfully!" << std::endl;
    return 0;
}
