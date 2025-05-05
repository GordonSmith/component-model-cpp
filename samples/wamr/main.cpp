#include <iostream>

#include "wasm_export.h"
#include "cmcpp.hpp"

using namespace cmcpp;

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

void trap(const char *msg)
{
    throw new std::runtime_error(msg);
}

std::pair<void *, size_t> convert(void *dest, uint32_t dest_byte_len, const void *src, uint32_t src_byte_len, Encoding from_encoding, Encoding to_encoding)
{
    if (from_encoding == to_encoding)
    {
        assert(dest_byte_len >= src_byte_len);
        if (src_byte_len > 0)
        {
            std::memcpy(dest, src, src_byte_len);
            return std::make_pair(dest, src_byte_len);
        }
        return std::make_pair(nullptr, 0);
    }
    assert(false);
}

std::vector<wasm_val_t> toWamr(const WasmValVector &values)
{
    std::vector<wasm_val_t> result(values.size());
    for (size_t i = 0; i < values.size(); i++)
    {
        switch (values[i].index())
        {
        case WASM_I32:
            result[i].kind = WASM_I32;
            result[i].of.i32 = std::get<int32_t>(values[i]);
            break;
        case WASM_I64:
            result[i].kind = WASM_I64;
            result[i].of.i64 = std::get<int64_t>(values[i]);
            break;
        case WASM_F32:
            result[i].kind = WASM_F32;
            result[i].of.f32 = std::get<float>(values[i]);
            break;
        case WASM_F64:
            result[i].kind = WASM_F64;
            result[i].of.f64 = std::get<double>(values[i]);
            break;
        default:
            assert(false);
        }
    }
    return result;
}

template <typename T>
WasmValVector fromWamr(size_t count, const wasm_val_t *values)
{
    auto size = ValTrait<T>::size;
    auto alignment = ValTrait<T>::alignment;
    auto target_count = ValTrait<T>::flat_types.size();

    WasmValVector result(count);
    for (size_t i = 0; i < count; i++)
    {
        switch (values[i].kind)
        {
        case WASM_I32:
            result[i] = values[i].of.i32;
            break;
        case WASM_I64:
            result[i] = values[i].of.i64;
            break;
        case WASM_F32:
            result[i] = values[i].of.f32;
            break;
        case WASM_F64:
            result[i] = values[i].of.f64;
            break;
        default:
            std::cout << "values[i].kind: " << values[i].kind << std::endl;
            assert(false);
        }
    }
    return result;
}

int main()
{
    char *buffer, error_buf[128];
    wasm_module_t module;
    wasm_module_inst_t module_inst;
    wasm_function_inst_t cabi_realloc;
    wasm_exec_env_t exec_env;
    uint32_t size, stack_size = 8092, heap_size = 8092;

    /* initialize the wasm runtime by default configurations */
    wasm_runtime_init();

    /* read WASM file into a memory buffer */
    buffer = read_wasm_binary_to_buffer("build/samples/wasm/src/wasm-build/sample.wasm", &size);

    // /* add line below if we want to export native functions to WASM app */
    // wasm_runtime_register_natives(...);

    // /* parse the WASM file from buffer and create a WASM module */
    module = wasm_runtime_load((uint8_t *)buffer, size, error_buf, sizeof(error_buf));

    // /* create an instance of the WASM module (WASM linear memory is ready) */
    module_inst = wasm_runtime_instantiate(module, stack_size, heap_size, error_buf, sizeof(error_buf));

    wasm_memory_inst_t memory = wasm_runtime_lookup_memory(module_inst, "memory");
    cabi_realloc = wasm_runtime_lookup_function(module_inst, "cabi_realloc");

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

    LiftLowerOptions opts(Encoding::Utf8, std::span<uint8_t>(static_cast<uint8_t *>(wasm_memory_get_base_address(memory)), 999999), realloc);

    LiftLowerContext liftLowerContext(trap, convert, opts);

    using and_func_t = func_t<bool_t(bool_t, bool_t)>;
    auto and_func = wasm_runtime_lookup_function(module_inst, "example:sample/booleans#and");
    and_func_t call_and = [&](bool_t a, bool_t b) -> bool_t
    {
        using params_t = ValTrait<and_func_t>::params_t;
        using results_t = ValTrait<and_func_t>::results_t;

        auto inputs = toWamr(lower_flat_values<params_t>(liftLowerContext, MAX_FLAT_PARAMS, {a, b}));
        auto output_size = 1;
        wasm_val_t outputs[output_size];
        auto call_result = wasm_runtime_call_wasm_a(exec_env, and_func, output_size, outputs, inputs.size(), inputs.data());
        auto result = std::get<0>(lift_flat_values<results_t>(liftLowerContext, MAX_FLAT_RESULTS, fromWamr<results_t>(output_size, outputs)));
        std::cout << "and_func(" << a << ", " << b << "): " << result << std::endl;
        return result;
    };
    call_and(false, false);
    call_and(false, true);
    call_and(true, false);
    call_and(true, true);

    using add_func_t = func_t<float64_t(float64_t, float64_t)>;
    auto add_func = wasm_runtime_lookup_function(module_inst, "example:sample/floats#add");
    add_func_t call_add = [&](float64_t input1, float64_t input2) -> float64_t
    {
        using params_t = ValTrait<add_func_t>::params_t;
        using results_t = ValTrait<add_func_t>::results_t;

        auto inputs = toWamr(lower_flat_values<params_t>(liftLowerContext, MAX_FLAT_PARAMS, {input1, input2}));
        auto output_size = 1;
        wasm_val_t outputs[output_size];
        auto call_result = wasm_runtime_call_wasm_a(exec_env, add_func, output_size, outputs, inputs.size(), inputs.data());
        auto result = std::get<0>(lift_flat_values<results_t>(liftLowerContext, MAX_FLAT_RESULTS, fromWamr<results_t>(output_size, outputs)));
        std::cout << "add_func(" << input1 << ", " << input2 << "): " << result << std::endl;
        return result;
    };
    call_add(3.1, 0.2);

    using reverse_func_t = func_t<string_t(string_t)>;
    auto reverse_func = wasm_runtime_lookup_function(module_inst, "example:sample/strings#reverse");
    auto reverse_cleanup_func = wasm_runtime_lookup_function(module_inst, "cabi_post_example:sample/strings#reverse");
    reverse_func_t call_reverse = [&](string_t input1) -> string_t
    {
        auto flat_ft_lower = func::flatten<reverse_func_t>(liftLowerContext, func::ContextType::Lower);
        auto flat_ft_lift = func::flatten<reverse_func_t>(liftLowerContext, func::ContextType::Lift);

        using params_t = ValTrait<reverse_func_t>::params_t;
        using results_t = ValTrait<reverse_func_t>::results_t;

        auto inputs = toWamr(lower_flat_values<params_t>(liftLowerContext, MAX_FLAT_PARAMS, {input1}));
        auto output_size = 1;
        wasm_val_t outputs[output_size];
        auto call_result = wasm_runtime_call_wasm_a(exec_env, reverse_func, output_size, outputs, inputs.size(), inputs.data());
        auto result = std::get<0>(lift_flat_values<results_t>(liftLowerContext, MAX_FLAT_RESULTS, fromWamr<results_t>(output_size, outputs)));
        std::cout << "reverse_string(" << input1 << "): " << result << std::endl;
        return result;
    };
    auto call_reverse_result = call_reverse("Hello World!");
    call_reverse(call_reverse_result);

    using lots_func_t = func_t<uint32_t(
        string_t, string_t, string_t, string_t, string_t, string_t, string_t, string_t,
        string_t, string_t, string_t, string_t, string_t, string_t, string_t, string_t, string_t)>;
    auto lots_func = wasm_runtime_lookup_function(module_inst, "example:sample/strings#lots");
    auto lots_cleanup_func = wasm_runtime_lookup_function(module_inst, "cabi_post_example:sample/strings#lots");
    lots_func_t call_lots = [&](string_t p1, string_t p2, string_t p3, string_t p4, string_t p5, string_t p6, string_t p7, string_t p8,
                                string_t p9, string_t p10, string_t p11, string_t p12, string_t p13, string_t p14, string_t p15, string_t p16, string_t p17) -> uint32_t
    {
        auto flat_ft_lower = func::flatten<lots_func_t>(liftLowerContext, func::ContextType::Lower);
        auto flat_ft_lift = func::flatten<lots_func_t>(liftLowerContext, func::ContextType::Lift);

        using params_t = ValTrait<lots_func_t>::params_t;
        using results_t = ValTrait<lots_func_t>::results_t;

        auto inputs = toWamr(lower_flat_values<params_t>(liftLowerContext, MAX_FLAT_PARAMS, {p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15, p16, p17}));
        auto inputs_size = inputs.size();
        auto output_size = 1;
        wasm_val_t outputs[output_size];
        auto call_result = wasm_runtime_call_wasm_a(exec_env, lots_func, output_size, outputs, inputs.size(), inputs.data());
        auto result = std::get<0>(lift_flat_values<results_t>(liftLowerContext, MAX_FLAT_RESULTS, fromWamr<results_t>(output_size, outputs)));
        std::cout << "lots_string(" << p1 << ", " << p2 << ", " << p3 << ", " << p4 << ", " << p5 << ", " << p6 << ", " << p7 << ", " << p8
                  << ", " << p9 << ", " << p10 << ", " << p11 << ", " << p12 << ", " << p13 << ", " << p14 << ", " << p15 << ", " << p16 << ", " << p17 << "): " << result << std::endl;
        return result;
    };
    auto call_lots_result = call_lots("p1", "p2", "p3", "p4", "p5", "p6", "p7", "p8", "p9", "p10", "p11", "p12", "p13", "p14", "p15", "p16", "p17");

    using reverse_tuple_func_t = func_t<tuple_t<string_t, bool_t>(tuple_t<bool_t, string_t>)>;
    auto reverse_tuple_func = wasm_runtime_lookup_function(module_inst, "example:sample/tuples#reverse");
    auto reverse_tuple_cleanup_func = wasm_runtime_lookup_function(module_inst, "cabi_post_example:sample/tuples#reverse");
    reverse_tuple_func_t call_reverse_tuple = [&](tuple_t<bool_t, string_t> a) -> tuple_t<string_t, bool_t>
    {
        using params_t = ValTrait<reverse_tuple_func_t>::params_t;
        using results_t = ValTrait<reverse_tuple_func_t>::results_t;

        auto inputs = toWamr(lower_flat_values<params_t>(liftLowerContext, 100 + MAX_FLAT_PARAMS, {a}));
        auto output_size = 1;
        wasm_val_t outputs[output_size];
        auto call_result = wasm_runtime_call_wasm_a(exec_env, reverse_tuple_func, output_size, outputs, inputs.size(), inputs.data());
        auto result = std::get<0>(lift_flat_values<results_t>(liftLowerContext, MAX_FLAT_RESULTS, fromWamr<results_t>(output_size, outputs)));
        std::cout << "reverse_tuple(" << std::get<0>(a) << ", " << std::get<1>(a) << "): " << std::get<0>(result) << ", " << std::get<1>(result) << std::endl;
        call_result = wasm_runtime_call_wasm_a(exec_env, reverse_tuple_cleanup_func, 0, nullptr, 1, outputs);
        return result;
    };
    auto call_reverse_tuple_result = call_reverse_tuple({false, "Hello World!"});
    // call_reverse_tuple({std::get<1>(call_reverse_tuple_result), std::get<0>(call_reverse_tuple_result}));

    using list_filter_bool_func_t = func_t<list_t<string_t>(list_t<variant_t<bool_t, string_t>>)>;
    auto list_filter_bool_func = wasm_runtime_lookup_function(module_inst, "example:sample/lists#filter-bool");
    auto list_filter_bool_cleanup_func = wasm_runtime_lookup_function(module_inst, "cabi_post_example:sample/lists#filter-bool");
    auto call_list_filter_bool = [&](list_t<variant_t<bool_t, string_t>> a) -> list_t<string_t>
    {
        using params_t = ValTrait<list_filter_bool_func_t>::params_t;
        using results_t = ValTrait<list_filter_bool_func_t>::results_t;

        auto inputs = toWamr(lower_flat_values<params_t>(liftLowerContext, MAX_FLAT_PARAMS, {a}));
        auto output_size = 1;
        wasm_val_t outputs[output_size];
        auto call_result = wasm_runtime_call_wasm_a(exec_env, list_filter_bool_func, output_size, outputs, inputs.size(), inputs.data());
        auto result = std::get<0>(lift_flat_values<results_t>(liftLowerContext, MAX_FLAT_RESULTS, fromWamr<results_t>(output_size, outputs)));
        std::cout << "list_filter_bool(" << a.size() << "): " << result.size() << std::endl;
        call_result = wasm_runtime_call_wasm_a(exec_env, list_filter_bool_cleanup_func, 0, nullptr, 1, outputs);
        return result;
    };
    auto call_list_filter_bool_result = call_list_filter_bool({{false}, {"Hello World!"}});

    wasm_runtime_destroy_exec_env(exec_env);
    wasm_runtime_deinstantiate(module_inst);
    wasm_runtime_unload(module);
    wasm_runtime_destroy();

    return 0;
}