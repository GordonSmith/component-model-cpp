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
    std::vector<wasm_val_t> result;
    result.reserve(values.size());

    for (const auto &val_variant : values)
    {
        result.emplace_back(std::visit([](auto &&arg) -> wasm_val_t
                                       {
            wasm_val_t w_val{}; // Value-initialize
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, int32_t>) {
                w_val.kind = WASM_I32;
                w_val.of.i32 = arg;
            } else if constexpr (std::is_same_v<T, int64_t>) {
                w_val.kind = WASM_I64;
                w_val.of.i64 = arg;
            } else if constexpr (std::is_same_v<T, float32_t>) {
                w_val.kind = WASM_F32;
                w_val.of.f32 = arg;
            } else if constexpr (std::is_same_v<T, float64_t>) {
                w_val.kind = WASM_F64;
                w_val.of.f64 = arg;
            } else {
                assert(false && "Unsupported type in WasmVal variant");
            }
            return w_val; }, val_variant));
    }
    return result;
}

WasmValVector fromWamr(size_t count, const wasm_val_t *values)
{
    WasmValVector result;
    result.reserve(count);

    for (size_t i = 0; i < count; ++i)
    {
        switch (values[i].kind)
        {
        case WASM_I32:
            result.emplace_back(values[i].of.i32);
            break;
        case WASM_I64:
            result.emplace_back(values[i].of.i64);
            break;
        case WASM_F32:
            result.emplace_back(values[i].of.f32);
            break;
        case WASM_F64:
            result.emplace_back(values[i].of.f64);
            break;
        default:
            assert(false && "fromWamr: Unexpected wasm_val_t kind.");
            break;
        }
    }
    return result;
}

template <typename F>
func_t<F> attach(const wasm_module_inst_t &module_inst, const wasm_exec_env_t &exec_env, LiftLowerContext &liftLowerContext, const char *name)
{
    using result_t = typename ValTrait<func_t<F>>::result_t;

    wasm_function_inst_t guest_func = wasm_runtime_lookup_function(module_inst, name);
    if (!guest_func)
    {
        wasm_module_inst_t current_module_inst = wasm_runtime_get_module_inst(exec_env);
        const char *exception = wasm_runtime_get_exception(current_module_inst);
        liftLowerContext.trap(exception ? exception : "Unable to find function");
    }
    wasm_function_inst_t guest_cleanup_func = wasm_runtime_lookup_function(module_inst, (std::string("cabi_post_") + name).c_str());

    return [guest_func, guest_cleanup_func, exec_env, &liftLowerContext](auto &&...args) -> result_t
    {
        WasmValVector lowered_args = lower_flat_values(
            liftLowerContext,
            MAX_FLAT_PARAMS,
            std::forward<decltype(args)>(args)...);
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

        if constexpr (output_size > 0)
        {
            auto output = lift_flat_values<result_t>(liftLowerContext, MAX_FLAT_RESULTS, fromWamr(output_size, outputs));

            if (guest_cleanup_func)
            {
                wasm_runtime_call_wasm_a(exec_env, guest_cleanup_func, 0, nullptr, output_size, outputs);
            }

            return output;
        }
    };
}

template <typename T>
constexpr char wamrSignatureChar()
{

    if constexpr (std::is_same_v<T, int32_t>)
    {
        return 'i';
    }
    else if constexpr (std::is_same_v<T, int64_t>)
    {
        return 'I';
    }
    else if constexpr (std::is_same_v<T, float32_t>)
    {
        return 'f';
    }
    else if constexpr (std::is_same_v<T, float64_t>)
    {
        return 'F';
    }
    else
    {
        assert(false && "Unsupported type in WasmVal variant");
        return '?';
    }
}

template <FlatValue R, FlatValue... Ps>
NativeSymbol wamrSymbol(const char *symbol, void *func_ptr, std::string &signature)
{
    NativeSymbol retVal;
    retVal.symbol = symbol;
    retVal.func_ptr = func_ptr;
    signature = "(";
    // Use an initializer list to expand the parameter pack
    (void)std::initializer_list<int>{(signature += wamrSignatureChar<Ps>(), 0)...};
    signature += ")";
    signature += wamrSignatureChar<R>();

    retVal.signature = signature.c_str();
    return retVal;
}

// template <typename F>
// bool host(LiftLowerContext &liftLowerContext, const char *name, func_t<F> &func)
// {
//     using params_t = typename ValTrait<func_t<F>>::params_t;
//     using result_t = typename ValTrait<func_t<F>>::result_t;
//     std::string signature;

//     auto lowered_func = [&liftLowerContext, &func](wasm_exec_env_t exec_env, auto... args)
//     {
//         auto params = lift_flat_values<params_t>(liftLowerContext, MAX_FLAT_PARAMS, std::forward<decltype(args)>(args)...);
//         auto result = func(std::forward<decltype(args)>(args)...);
//     };
//     auto symbol = [&]<std::size_t... I>(std::index_sequence<I...>)
//     {
//         return wamrSymbol<result_t, std::remove_reference_t<std::tuple_element_t<I, params_t>>...>(name, (void *)&lowered_func, signature);
//     }(std::make_index_sequence<ValTrait<result_t>::flat_types.size()>{});

//     return wasm_runtime_register_natives(name, &symbol, 1);
// }

using MyFunc = std::function<void(wasm_exec_env_t, int32_t, unsigned char *, size_t, unsigned char *)>;
MyFunc my_func = [](wasm_exec_env_t exec_env, int32_t a, unsigned char *b, size_t c, unsigned char *d)
{
    throw std::runtime_error("test");
};

template <typename F>
void reverse(wasm_exec_env_t exec_env, uint64_t *args)
{
    using params_t = typename ValTrait<func_t<F>>::params_t;
    using result_t = typename ValTrait<func_t<F>>::result_t;

    native_raw_return_type(int32_t, args);

    for (int i = 0; i < ValTrait<result_t>::flat_types.size(); i++)
    {
        std::cout << i << std::endl;
        // native_raw_return_type(ValTrait<result_t>::flat_types[i], args);
    }
    for (int i = 0; i < ValTrait<params_t>::flat_types.size(); i++)
    {
        std::cout << i << std::endl;
        // native_raw_return_type(ValTrait<params_t>::flat_types[i], args);
    }
    native_raw_get_arg(int, x, args);
    native_raw_get_arg(int, y, args);
    void *attachment = wasm_runtime_get_function_attachment(exec_env);
    int res = x + y;
    native_raw_set_return(res);
}
int mol = 42;
NativeSymbol symbol[] = {{"reverse", (void *)reverse<tuple_t<string_t, bool_t>(tuple_t<bool_t, string_t>)>, "(i*i*)", &mol}};

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

    bool success = wasm_runtime_register_natives_raw("example:sample/tuples", symbol, 1);

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

    auto variant_func = attach<variant_t<bool_t, uint32_t>(variant_t<bool_t, uint32_t>)>(module_inst, exec_env, liftLowerContext, "example:sample/variants#variant-func");
    std::cout << "variant_func((uint32_t)40)" << std::get<1>(variant_func((uint32_t)40)) << std::endl;
    std::cout << "variant_func((bool_t)true)" << std::get<0>(variant_func((bool_t) true)) << std::endl;
    std::cout << "variant_func((bool_t)false)" << std::get<0>(variant_func((bool_t) false)) << std::endl;

    auto option_func = attach<option_t<uint32_t>(option_t<uint32_t>)>(module_inst, exec_env, liftLowerContext, "option-func");
    std::cout << "option_func((uint32_t)40).has_value()" << option_func((uint32_t)40).has_value() << std::endl;
    std::cout << "option_func((uint32_t)40).value()" << option_func((uint32_t)40).value() << std::endl;
    std::cout << "option_func(std::nullopt).has_value()" << option_func(std::nullopt).has_value() << std::endl;

    auto void_func = attach<void()>(module_inst, exec_env, liftLowerContext, "void-func");
    void_func();

    auto ok_func = attach<result_t<uint32_t, string_t>(uint32_t, uint32_t)>(module_inst, exec_env, liftLowerContext, "ok-func");
    auto ok_func_result = ok_func(40, 2);
    std::cout << "ok_func result: " << std::get<uint32_t>(ok_func_result) << std::endl;

    auto err_func = attach<result_t<uint32_t, string_t>(uint32_t, uint32_t)>(module_inst, exec_env, liftLowerContext, "err-func");
    auto err_func_result = err_func(40, 2);
    std::cout << "err_func result: " << std::get<string_t>(err_func_result) << std::endl;

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

    func_t<tuple_t<string_t, bool_t>(tuple_t<bool_t, string_t>)> host_reverse_tuple = [](tuple_t<bool_t, string_t> a) -> tuple_t<string_t, bool_t>
    {
        return {std::get<string_t>(a), std::get<bool_t>(a)};
    };
    // auto xxxx = host<tuple_t<string_t, bool_t>(tuple_t<bool_t, string_t>)>(liftLowerContext,
    //                                                                        "example:sample/tuples#reverse",
    //                                                                        host_reverse_tuple);
    auto call_reverse_tuple = attach<tuple_t<string_t, bool_t>(tuple_t<bool_t, string_t>)>(module_inst, exec_env, liftLowerContext,
                                                                                           "example:sample/tuples#reverse");
    auto call_reverse_tuple_result = call_reverse_tuple({false, "Hello World!"});
    std::cout << "call_reverse_tuple({false, \"Hello World!\"}): " << std::get<0>(call_reverse_tuple_result) << ", " << std::get<1>(call_reverse_tuple_result) << std::endl;

    auto call_list_filter = attach<list_t<string_t>(list_t<variant_t<bool_t, string_t>>)>(module_inst, exec_env, liftLowerContext, "example:sample/lists#filter-bool");
    auto call_list_filter_result = call_list_filter({{false}, {"Hello World!"}, {"Another String"}, {true}, {false}});
    std::cout << "call_list_filter result: " << call_list_filter_result.size() << std::endl;

    enum e
    {
        a,
        b,
        c
    };

    auto enum_func = attach<enum_t<e>(enum_t<e>)>(module_inst, exec_env, liftLowerContext, "example:sample/enums#enum-func");
    std::cout << "enum_func(e::a): " << enum_func(e::a) << std::endl;
    std::cout << "enum_func(e::b): " << enum_func(e::b) << std::endl;
    std::cout << "enum_func(e::c): " << enum_func(e::c) << std::endl;

    wasm_runtime_destroy_exec_env(exec_env);
    wasm_runtime_deinstantiate(module_inst);
    wasm_runtime_unload(module);
    wasm_runtime_destroy();

    return 0;
}
