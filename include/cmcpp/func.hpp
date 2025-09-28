#ifndef CMCPP_FUNC_HPP
#define CMCPP_FUNC_HPP

#include "context.hpp"
#include "flags.hpp"

namespace cmcpp
{
    namespace func
    {
        enum class ContextType
        {
            Lift,
            Lower
        };

        template <Flags T>
        inline int32_t pack_flags_into_int(const T &v)
        {
            return flags::pack_flags_into_int(v);
        }

        template <Flags T>
        inline void store(LiftLowerContext &cx, const T &v, offset ptr)
        {
            flags::store(cx, v, ptr);
        }

        template <Flags T>
        inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v)
        {
            return flags::lower_flat(cx, v);
        }

        template <Flags T>
        inline T unpack_flags_from_int(uint32_t value)
        {
            return flags::unpack_flags_from_int<T>(value);
        }

        template <Flags T>
        inline T load(const LiftLowerContext &cx, uint32_t ptr)
        {
            return flags::load<T>(cx, ptr);
        }

        template <Flags T>
        inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
        {
            return flags::lift_flat<T>(cx, vi);
        }

        template <Func T>
        inline core_func_t flatten(const CanonicalOptions &opts, ContextType context)
        {
            using params_trait = ValTrait<typename ValTrait<T>::params_t>;
            using result_trait = ValTrait<typename ValTrait<T>::result_t>;

            WasmValTypeVector flat_params(params_trait::flat_types.begin(), params_trait::flat_types.end());
            WasmValTypeVector flat_results(result_trait::flat_types.begin(), result_trait::flat_types.end());

            const size_t raw_param_count = flat_params.size();
            const size_t raw_result_count = flat_results.size();

            auto pointer_type = []() -> WasmValTypeVector
            {
                return {WasmValType::i32};
            };

            if (opts.sync)
            {
                if (raw_param_count > MAX_FLAT_PARAMS)
                {
                    flat_params = pointer_type();
                }

                if (raw_result_count > MAX_FLAT_RESULTS)
                {
                    if (context == ContextType::Lift)
                    {
                        flat_results = pointer_type();
                    }
                    else
                    {
                        flat_params.push_back(WasmValType::i32);
                        flat_results.clear();
                    }
                }
            }
            else
            {
                if (context == ContextType::Lift)
                {
                    if (raw_param_count > MAX_FLAT_PARAMS)
                    {
                        flat_params = pointer_type();
                    }

                    if (opts.callback.has_value())
                    {
                        flat_results = pointer_type();
                    }
                    else
                    {
                        flat_results.clear();
                    }
                }
                else
                {
                    if (raw_param_count > MAX_FLAT_ASYNC_PARAMS)
                    {
                        flat_params = pointer_type();
                    }

                    if (raw_result_count > 0)
                    {
                        flat_params.push_back(WasmValType::i32);
                    }

                    flat_results = pointer_type();
                }
            }

            return {std::move(flat_params), std::move(flat_results)};
        }
    }
}
#endif
