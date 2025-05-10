#include "wasm_export.h"
#include "cmcpp.hpp"

#include <iostream>

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

template <typename F>
func_t<F> attach(const wasm_module_inst_t &module_inst, const wasm_exec_env_t &exec_env, LiftLowerContext &liftLowerContext, const char *name)
{
    using params_t = typename ValTrait<func_t<F>>::params_t;
    using result_t = typename ValTrait<func_t<F>>::result_t;

    wasm_function_inst_t guest_func = wasm_runtime_lookup_function(module_inst, name);
    wasm_function_inst_t guest_cleanup_func = wasm_runtime_lookup_function(module_inst, (std::string("cabi_post_") + name).c_str());

    return [guest_func, guest_cleanup_func, exec_env, &liftLowerContext](auto &&...args) -> auto
    {
        WasmValVector lowered_args = lower_flat_values<params_t>(
            liftLowerContext,
            MAX_FLAT_PARAMS,
            {std::forward<decltype(args)>(args)...});
        std::vector<wasm_val_t> inputs = toWamr(lowered_args);

        constexpr size_t output_size = std::is_same<result_t, void>::value ? 0 : 1;
        wasm_val_t outputs[output_size];

        bool success = wasm_runtime_call_wasm_a(exec_env, guest_func,
                                                output_size, outputs,
                                                inputs.size(), inputs.data());

        if (!success)
        {
            wasm_module_inst_t current_module_inst = wasm_runtime_get_module_inst(exec_env);
            const char *exception = wasm_runtime_get_exception(current_module_inst);
            liftLowerContext.trap(exception ? exception : "Unknown WAMR execution error");
        }

        WasmValVector flat_results = fromWamr<result_t>(output_size, outputs);
        auto output = lift_flat_values<result_t>(liftLowerContext, MAX_FLAT_RESULTS, flat_results);

        if (guest_cleanup_func)
        {
            wasm_runtime_call_wasm_a(exec_env, guest_cleanup_func, 0, nullptr, output_size, outputs);
        }

        return output;
    };
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

    auto call_and = attach<bool_t(bool_t, bool_t)>(module_inst, exec_env, liftLowerContext,
                                                   "example:sample/booleans#and");
    std::cout << "call_and(false, false): " << call_and(false, false) << std::endl;
    std::cout << "call_and(false, true): " << call_and(false, true) << std::endl;
    std::cout << "call_and(true, false): " << call_and(true, false) << std::endl;
    std::cout << "call_and(true, true): " << call_and(true, true) << std::endl;

    auto call_add = attach<float64_t(float64_t, float64_t)>(module_inst, exec_env, liftLowerContext,
                                                            "example:sample/floats#add");
    std::cout << "call_add(3.1, 0.2): " << call_add(3.1, 0.2) << std::endl;
    std::cout << "call_add(1.5, 2.5): " << call_add(1.5, 2.5) << std::endl;

    auto call_reverse = attach<string_t(string_t)>(module_inst, exec_env, liftLowerContext,
                                                   "example:sample/strings#reverse");
    auto call_reverse_result = call_reverse("Hello World!");
    std::cout << "call_reverse(\"Hello World!\"): " << call_reverse_result << std::endl;
    std::cout << "call_reverse(call_reverse(\"Hello World!\")): " << call_reverse(call_reverse_result) << std::endl;

    auto call_lots = attach<uint32_t(string_t, string_t, string_t, string_t, string_t, string_t, string_t, string_t, string_t, string_t, string_t, string_t, string_t, string_t, string_t, string_t, string_t)>(
        module_inst, exec_env, liftLowerContext,
        "example:sample/strings#lots");

    auto call_lots_result = call_lots(
        "p1", "p2", "p3", "p4", "p5", "p6", "p7", "p8",
        "p9", "p10", "p11", "p12", "p13", "p14", "p15", "p16", "p17");
    std::cout << "call_lots result: " << call_lots_result << std::endl;

    auto call_reverse_tuple = attach<tuple_t<string_t, bool_t>(tuple_t<bool_t, string_t>)>(module_inst, exec_env, liftLowerContext,
                                                                                           "example:sample/tuples#reverse");
    auto call_reverse_tuple_result = call_reverse_tuple({false, "Hello World!"});
    std::cout << "call_reverse_tuple({false, \"Hello World!\"}): " << std::get<0>(call_reverse_tuple_result) << ", " << std::get<1>(call_reverse_tuple_result) << std::endl;

    auto call_list_filter = attach<list_t<string_t>(list_t<variant_t<bool_t, string_t>>)>(module_inst, exec_env, liftLowerContext, "example:sample/lists#filter-bool");
    auto call_list_filter_result = call_list_filter({{false}, {"Hello World!"}, {"Another String"}, {true}, {false}});
    std::cout << "call_list_filter result: " << call_list_filter_result.size() << std::endl;

    wasm_runtime_destroy_exec_env(exec_env);
    wasm_runtime_deinstantiate(module_inst);
    wasm_runtime_unload(module);
    wasm_runtime_destroy();

    return 0;
}
