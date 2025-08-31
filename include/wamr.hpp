#ifndef CMCPP_WAMR_HPP
#define CMCPP_WAMR_HPP

#include "wasm_export.h"
#include "cmcpp.hpp"

namespace cmcpp
{

    inline void trap(const char *msg)
    {
        throw new std::runtime_error(msg);
    }

    inline std::vector<wasm_val_t> wasmVal2wam_val_t(const WasmValVector &values)
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

    inline WasmValVector wam_val_t2wasmVal(size_t count, const wasm_val_t *values)
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
    func_t<F> guest_function(const wasm_module_inst_t &module_inst, const wasm_exec_env_t &exec_env, LiftLowerContext &liftLowerContext, const char *name)
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
                nullptr,
                std::forward<decltype(args)>(args)...);
            std::vector<wasm_val_t> inputs = wasmVal2wam_val_t(lowered_args);

            constexpr size_t output_size = std::is_same<result_t, void>::value ? 0 : 1;
            wasm_val_t outputs[output_size == 0 ? 1 : output_size];

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
                auto output = lift_flat_values<result_t>(liftLowerContext, MAX_FLAT_RESULTS, wam_val_t2wasmVal(output_size, outputs));

                if (guest_cleanup_func)
                {
                    wasm_runtime_call_wasm_a(exec_env, guest_cleanup_func, 0, nullptr, output_size, outputs);
                }

                return output;
            }
        };
    }

    inline std::pair<void *, size_t> convert(void *dest, uint32_t dest_byte_len, const void *src, uint32_t src_byte_len, Encoding from_encoding, Encoding to_encoding)
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

    template <Field T>
    WasmValVector rawWamrArg2Wasm(wasm_exec_env_t exec_env, uint64_t *&args)
    {
        WasmValVector retVal;
        for (int i = 0; i < ValTrait<T>::flat_types.size(); i++)
        {
            switch (ValTrait<T>::flat_types[i])
            {
            case WasmValType::i32:
            {
                native_raw_get_arg(int32_t, p, args);
                retVal.push_back(p);
                break;
            }
            case WasmValType::i64:
            {
                native_raw_get_arg(int64_t, p, args);
                retVal.push_back(p);
                break;
            }
            case WasmValType::f32:
            {
                native_raw_get_arg(float32_t, p, args);
                retVal.push_back(p);
                break;
            }
            case WasmValType::f64:
            {
                native_raw_get_arg(float64_t, p, args);
                retVal.push_back(p);
                break;
            }
            }
        }
        return retVal;
    }

    template <typename F>
    void export_func(wasm_exec_env_t exec_env, uint64_t *args)
    {
        using result_t = typename ValTrait<func_t<F>>::result_t;
        uint64_t *orig_raw_ret = args;
        std::function<F> *func = (std::function<F> *)wasm_runtime_get_function_attachment(exec_env);

        using params_t_outer = typename ValTrait<func_t<F>>::params_t;
        using params_t = typename ValTrait<params_t_outer>::inner_type;
        auto lower_params = rawWamrArg2Wasm<params_t>(exec_env, args);

        wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
        wasm_memory_inst_t memory = wasm_runtime_lookup_memory(module_inst, "memory");
        wasm_function_inst_t cabi_realloc = wasm_runtime_lookup_function(module_inst, "cabi_realloc");
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
        auto params = lift_flat_values<params_t>(liftLowerContext, MAX_FLAT_PARAMS, lower_params);

        if constexpr (ValTrait<result_t>::flat_types.size() > 0)
        {
            using lower_result_t = typename WasmValTypeTrait<ValTrait<result_t>::flat_types[0]>::type;
            native_raw_return_type(lower_result_t, orig_raw_ret);
            result_t result = std::apply(*func, params);
            native_raw_get_arg(uint32_t, out_param, args);
            auto lower_results = lower_flat_values<result_t>(liftLowerContext, MAX_FLAT_RESULTS, &out_param, std::forward<result_t>(result));
            if (lower_results.size() > 0)
            {
                auto lower_result = std::get<lower_result_t>(lower_results[0]);
                native_raw_set_return(lower_result);
            }
        }
        else
        {
            std::apply(*func, params);
        }
    }

    template <typename F>
    NativeSymbol host_function(const char *name, F &func)
    {
        static std::function<F> func_wrapper(func);
        NativeSymbol symbol = {
            name,
            (void *)export_func<F>,
            NULL,
            (void *)&func_wrapper};
        return symbol;
    }

    inline bool host_module(const char *module_name, NativeSymbol *native_symbols, uint32_t n_native_symbols)
    {
        return wasm_runtime_register_natives_raw(module_name, native_symbols, n_native_symbols);
    }

}

#endif