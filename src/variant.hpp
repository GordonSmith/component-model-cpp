#ifndef CMCPP_VARIANT_HPP
#define CMCPP_VARIANT_HPP

#include "context.hpp"
#include "integer.hpp"
#include "float.hpp"
#include "store.hpp"
#include "load.hpp"
#include "util.hpp"

#include <tuple>
#include <limits>
#include <cassert>

namespace cmcpp
{
    namespace variant
    {

        template <Variant T>
        std::size_t max_case_alignment(const T &v)
        {
            std::size_t a = 1;
            std::visit([&](auto &&arg)
                       {
                using V = std::decay_t<decltype(arg)>;
                a = std::max(a, alignment(arg)); }, v);
            return a;
        }

        struct LowerFlatVisitor
        {
            CallContext &cx;
            template <typename T>
            WasmValVector operator()(T &&arg) const
            {
                return lower_flat(cx, std::forward<T>(arg));
            }
        };

        template <Variant T>
        WasmValVector lower_flat_variant(CallContext &cx, const T &v)
        {
            auto case_index = v.index();
            WasmValTypeVector flat_types(ValTrait<T>::flat_types.begin(), ValTrait<T>::flat_types.end());
            auto top = flat_types.front();
            flat_types.erase(flat_types.begin());
            assert(top == WasmValType::i32);
            auto payload = std::visit(LowerFlatVisitor{cx}, v);

            auto payload_iter = payload.begin();
            auto have_types = ValTrait<T>::flat_types;
            auto have_types_iter = have_types.begin();
            size_t i = 0;
            while (payload_iter != payload.end() && have_types_iter != have_types.end())
            {
                auto want = flat_types.front();
                flat_types.erase(flat_types.begin());
                auto have = *have_types_iter;
                if (have == WasmValType::f32 && want == WasmValType::i32)
                {
                    payload[i] = float_::encode_float_as_i32(std::get<float32_t>(*payload_iter));
                }
                else if (have == WasmValType::i32 && want == WasmValType::i64)
                {
                    payload[i] = static_cast<int64_t>(std::get<int32_t>(*payload_iter));
                }
                else if (have == WasmValType::f32 && want == WasmValType::i64)
                {
                    payload[i] = static_cast<int64_t>(float_::encode_float_as_i32(std::get<float32_t>(*payload_iter)));
                }
                else if (have == WasmValType::f64 && want == WasmValType::i64)
                {
                    payload[i] = static_cast<int64_t>(std::get<float64_t>(*payload_iter));
                }
                else
                {
                    assert(have == want);
                }
                ++payload_iter;
                ++have_types_iter;
                ++i;
            }
            for (auto itr = flat_types.begin(); itr != flat_types.end(); ++itr)
            {
                payload.push_back(0);
            }
            payload.insert(payload.begin(), static_cast<int32_t>(case_index));
            return payload;
        }

        template <Variant T, std::size_t Index = 0>
        T lift_flat_helper(const CallContext &cx, const CoreValueIter &vi, int32_t case_index)
        {
            if (case_index == Index)
            {
                using CaseType = std::variant_alternative_t<Index, T>;
                auto value = lift_flat<CaseType>(cx, vi);
                return T(std::in_place_index<Index>, value);
            }
            else if constexpr (Index + 1 < std::variant_size_v<T>)
            {
                return lift_flat_helper<T, Index + 1>(cx, vi, case_index);
            }
            else
            {
                // This should not happen if the earlier trap_if check is correct
                throw std::runtime_error("Invalid variant case index");
            }
        }

        template <Variant T>
        inline T lift_flat(const CallContext &cx, const CoreValueIter &vi)
        {
            WasmValTypeVector flat_types(ValTrait<T>::flat_types.begin(), ValTrait<T>::flat_types.end());
            auto top = flat_types.front();
            flat_types.erase(flat_types.begin());
            assert(top == WasmValType::i32);
            int32_t case_index = vi.next<int32_t>();
            trap_if(cx, case_index >= std::variant_size_v<T>);
            CoerceValueIter cvi(vi, flat_types);
            return lift_flat_helper<T>(cx, cvi, case_index);
        }
    }
}

#endif
