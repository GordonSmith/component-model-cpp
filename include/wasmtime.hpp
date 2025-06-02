#pragma once

#include <wasmtime.hh>
#include "cmcpp.hpp"
#include <iostream>

namespace cmcpp
{

    void trap(const char *msg)
    {
        throw std::runtime_error(msg);
    }

    std::vector<wasmtime_val_t> wasmVal2wam_val_t(const WasmValVector &values)
    {
        std::vector<wasmtime_val_t> result;
        result.reserve(values.size());

        for (const auto &val_variant : values)
        {
            result.emplace_back(std::visit([](auto &&arg) -> wasmtime_val_t
                                           {
            wasmtime_val_t w_val{}; // Value-initialize
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

    WasmValVector wam_val_t2wasmVal(size_t count, const wasmtime_val_t *values)
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
                assert(false && "fromWasmtime: Unexpected wasmtime_val_t kind.");
                break;
            }
        }
        return result;
    }

    void handle_error(const char *message, wasmtime_error_t *error = nullptr, wasm_trap_t *trap = nullptr)
    {
        std::cerr << "Error: " << message << std::endl;

        if (error)
        {
            wasm_byte_vec_t error_message;
            wasmtime_error_message(error, &error_message);
            std::cerr << "Error details: " << std::string(error_message.data, error_message.size) << std::endl;
            wasm_byte_vec_delete(&error_message);
        }

        if (trap)
        {
            wasm_byte_vec_t trap_message;
            wasm_trap_message(trap, &trap_message);
            std::cerr << "Trap details: " << std::string(trap_message.data, trap_message.size) << std::endl;
            wasm_byte_vec_delete(&trap_message);
        }
    }

    template <typename F>
    func_t<F> guest_function(const wasmtime_linker_t *linker, wasmtime_context_t *context, LiftLowerContext &liftLowerContext, const char *name)
    {
        using result_t = typename ValTrait<func_t<F>>::result_t;

        wasmtime_extern_t guest_func;
        bool found = wasmtime_linker_get(linker, context, "", 0, name, strlen(name), &guest_func);
        if (!found || guest_func.kind != WASMTIME_EXTERN_FUNC)
        {
            liftLowerContext.trap("Unable to find function");
        }
        std::string cleanup_name = "cabi_post_";
        cleanup_name += name;
        wasmtime_extern_t guest_cleanup_func;
        wasmtime_linker_get(linker, context, "", 0, cleanup_name.c_str(), cleanup_name.size(), &guest_cleanup_func);

        return [guest_func, guest_cleanup_func, context, &liftLowerContext](auto &&...args) -> result_t
        {
            WasmValVector lowered_args = lower_flat_values(
                liftLowerContext,
                MAX_FLAT_PARAMS,
                nullptr,
                std::forward<decltype(args)>(args)...);
            std::vector<wasmtime_val_t> inputs = wasmVal2wam_val_t(lowered_args);

            constexpr size_t output_size = std::is_same<result_t, void>::value ? 0 : 1;
            wasmtime_val_t outputs[output_size];

            wasm_trap_t *trap = nullptr;
            wasmtime_error_t *error = wasmtime_func_call(context, guest_func.of.func, inputs.data(), inputs.size(), outputs, output_size, &trap);

            if (error)
            {
                handle_error("Function call failed", error);
                wasmtime_error_delete(error);
                liftLowerContext.trap("Function call failed");
            }
            if (trap)
            {
                handle_error("Function call trapped", nullptr, trap);
                wasm_trap_delete(trap);
                throw std::runtime_error("Function call trapped");
            }

            if constexpr (output_size > 0)
            {
                auto output = lift_flat_values<result_t>(liftLowerContext, MAX_FLAT_RESULTS, wam_val_t2wasmVal(output_size, outputs));

                if (guest_cleanup_func.kind == WASMTIME_EXTERN_FUNC)
                {
                    wasmtime_error_t *error = wasmtime_func_call(context, guest_cleanup_func.of.func, nullptr, 0, outputs, output_size, &trap);
                    if (error)
                    {
                        handle_error("Function call failed", error);
                        wasmtime_error_delete(error);
                        liftLowerContext.trap("Function call failed");
                    }
                    if (trap)
                    {
                        handle_error("Function call trapped", nullptr, trap);
                        wasm_trap_delete(trap);
                        throw std::runtime_error("Function call trapped");
                    }
                }

                return output;
            }
        };
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

    // Helper to create a LiftLowerContext from wasmtime store and instance
    LiftLowerContext create_lift_lower_context(wasmtime_context_t *context, wasmtime_instance_t *instance)
    {
        // Get memory export
        wasmtime_extern_t memory_extern;
        bool found = wasmtime_instance_export_get(context, instance, "memory", 6, &memory_extern);
        if (!found || memory_extern.kind != WASMTIME_EXTERN_MEMORY)
        {
            throw std::runtime_error("Memory export not found");
        }

        wasmtime_memory_t memory = memory_extern.of.memory;
        uint8_t *data = wasmtime_memory_data(context, &memory);
        size_t size = wasmtime_memory_data_size(context, &memory);

        // Get cabi_realloc function
        wasmtime_extern_t realloc_extern;
        found = wasmtime_instance_export_get(context, instance, "cabi_realloc", 12, &realloc_extern);

        GuestRealloc realloc = nullptr;
        if (found && realloc_extern.kind == WASMTIME_EXTERN_FUNC)
        {
            wasmtime_func_t realloc_func = realloc_extern.of.func;
            realloc = [context, realloc_func](int original_ptr, int original_size, int alignment, int new_size) -> int
            {
                wasmtime_val_t params[4] = {
                    {.kind = WASM_I32, .of = {.i32 = original_ptr}},
                    {.kind = WASM_I32, .of = {.i32 = original_size}},
                    {.kind = WASM_I32, .of = {.i32 = alignment}},
                    {.kind = WASM_I32, .of = {.i32 = new_size}}};
                wasmtime_val_t result[1];
                wasm_trap_t *trap = nullptr;
                wasmtime_error_t *error = wasmtime_func_call(context, &realloc_func, params, 4, result, 1, &trap);
                if (error || trap)
                {
                    throw std::runtime_error("cabi_realloc failed");
                }
                return result[0].of.i32;
            };
        }

        LiftLowerOptions opts(Encoding::Utf8, std::span<uint8_t>(data, size), realloc);
        return LiftLowerContext(trap, convert, opts);
    }

    // Host function wrapper for wasmtime
    template <typename F>
    class HostFunctionWrapper
    {
    public:
        using func_type = F;
        using result_t = typename ValTrait<func_t<F>>::result_t;
        using params_t_outer = typename ValTrait<func_t<F>>::params_t;
        using params_t = typename ValTrait<params_t_outer>::inner_type;

        static wasm_trap_t *callback(
            void *env,
            wasmtime_caller_t *caller,
            const wasmtime_val_t *args,
            size_t nargs,
            wasmtime_val_t *results,
            size_t nresults)
        {
            try
            {
                auto *func = static_cast<std::function<F> *>(env);

                // Get context from caller
                wasmtime_context_t *context = wasmtime_caller_context(caller);

                // Get instance from caller (this is a simplified approach)
                // In real usage, you might need to store the instance reference elsewhere
                wasmtime_extern_t instance_extern;
                wasmtime_instance_t *instance = nullptr;

                // Create lift/lower context
                auto liftLowerContext = create_lift_lower_context(context, instance);

                // Convert args to WasmValVector
                WasmValVector wasm_args = wam_val_t2wasmVal(nargs, args);

                // Lift parameters
                auto params = lift_flat_values<params_t>(liftLowerContext, MAX_FLAT_PARAMS, wasm_args);

                if constexpr (!std::is_same_v<result_t, void>)
                {
                    // Call function and get result
                    result_t result = std::apply(*func, params);

                    // Lower result
                    auto lowered_results = lower_flat_values(
                        liftLowerContext,
                        MAX_FLAT_RESULTS,
                        nullptr,
                        std::forward<result_t>(result));

                    // Convert to wasmtime values
                    auto wasmtime_results = wasmVal2wam_val_t(lowered_results);

                    // Copy results
                    for (size_t i = 0; i < std::min(wasmtime_results.size(), nresults); ++i)
                    {
                        results[i] = wasmtime_results[i];
                    }
                }
                else
                {
                    // Call void function
                    std::apply(*func, params);
                }

                return nullptr; // No trap
            }
            catch (const std::exception &e)
            {
                wasm_message_t message;
                message.data = const_cast<wasm_byte_t *>(reinterpret_cast<const wasm_byte_t *>(e.what()));
                message.size = strlen(e.what());
                return wasmtime_trap_new(message.data, message.size);
            }
        }
    };

    template <typename F>
    wasmtime_func_t *host_function(
        wasmtime_context_t *context,
        const char *name,
        F &func)
    {
        static std::function<F> func_wrapper(func);

        // Create function type
        using wrapper = HostFunctionWrapper<F>;
        using params_t_outer = typename wrapper::params_t_outer;
        using result_t = typename wrapper::result_t;

        // Get parameter and result types
        constexpr size_t param_count = ValTrait<params_t_outer>::flat_types.size();
        constexpr size_t result_count = std::is_same_v<result_t, void> ? 0 : ValTrait<result_t>::flat_types.size();

        wasm_valtype_vec_t params;
        wasm_valtype_vec_t results;

        // Initialize parameter types
        wasm_valtype_t *param_types[param_count];
        for (size_t i = 0; i < param_count; ++i)
        {
            switch (ValTrait<params_t_outer>::flat_types[i])
            {
            case WasmValType::i32:
                param_types[i] = wasm_valtype_new_i32();
                break;
            case WasmValType::i64:
                param_types[i] = wasm_valtype_new_i64();
                break;
            case WasmValType::f32:
                param_types[i] = wasm_valtype_new_f32();
                break;
            case WasmValType::f64:
                param_types[i] = wasm_valtype_new_f64();
                break;
            }
        }
        wasm_valtype_vec_new(&params, param_count, param_types);

        // Initialize result types
        if constexpr (result_count > 0)
        {
            wasm_valtype_t *result_types[result_count];
            for (size_t i = 0; i < result_count; ++i)
            {
                switch (ValTrait<result_t>::flat_types[i])
                {
                case WasmValType::i32:
                    result_types[i] = wasm_valtype_new_i32();
                    break;
                case WasmValType::i64:
                    result_types[i] = wasm_valtype_new_i64();
                    break;
                case WasmValType::f32:
                    result_types[i] = wasm_valtype_new_f32();
                    break;
                case WasmValType::f64:
                    result_types[i] = wasm_valtype_new_f64();
                    break;
                }
            }
            wasm_valtype_vec_new(&results, result_count, result_types);
        }
        else
        {
            wasm_valtype_vec_new_empty(&results);
        }

        // Create function type
        wasm_functype_t *functype = wasm_functype_new(&params, &results);

        // Create function
        wasm_func_callback_with_env_t callback = wrapper::callback;
        wasmtime_func_t *func_obj = wasmtime_func_new_with_env(
            context,
            functype,
            callback,
            &func_wrapper,
            nullptr);

        wasm_functype_delete(functype);

        return func_obj;
    }

}
