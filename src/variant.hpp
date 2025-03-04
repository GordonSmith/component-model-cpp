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
        template <Variant V>
        struct LoadVisitor
        {
            const CallContext &cx;
            V &variant;
            size_t index_to_set;
            size_t current_index;
            int ptr;

            LoadVisitor(const CallContext &cx, V &var, size_t idx, uint32_t ptr)
                : cx(cx), variant(var), index_to_set(idx), current_index(0), ptr(ptr) {}

            template <typename T>
            void operator()(T &)
            {
                if (current_index == index_to_set)
                {
                    variant = load<T>(cx, ptr);
                }
                ++current_index;
            }
        };

        template <Variant T>
        T load(const CallContext &cx, uint32_t ptr)
        {
            T retVal;
            uint32_t disc_size = ValTrait<typename ValTrait<T>::discriminant_type>::size;
            auto case_index = integer::load<typename ValTrait<T>::discriminant_type>(cx, ptr);
            ptr += disc_size;
            trap_if(cx, case_index >= std::variant_size_v<T>);
            ptr = align_to(ptr, ValTrait<T>::max_case_alignment);
            LoadVisitor<T> visitor(cx, retVal, case_index, ptr);
            std::visit(visitor, retVal);
            return retVal;
        }

        struct StoreVisitor
        {
            CallContext &cx;
            uint32_t ptr;

            template <typename T>
            void operator()(T &&arg) const
            {
                store(cx, arg, ptr);
            }
        };

        template <Variant T>
        void store(CallContext &cx, const T &v, uint32_t ptr)
        {
            auto case_index = v.index();
            uint32_t disc_size = ValTrait<typename ValTrait<T>::discriminant_type>::size;
            integer::store(cx, case_index, ptr, disc_size);
            ptr += disc_size;
            ptr = align_to(ptr, ValTrait<T>::max_case_alignment);
            std::visit(StoreVisitor{cx, ptr}, v);
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

    template <Variant T>
    inline void store(CallContext &cx, const T &v, uint32_t ptr)
    {
        variant::store(cx, v, ptr);
    }

    template <Variant T>
    inline T load(const CallContext &cx, uint32_t ptr)
    {
        return variant::load<T>(cx, ptr);
    }
}

#endif
