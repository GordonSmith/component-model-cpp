#ifndef CMCPP_VARIANT_HPP
#define CMCPP_VARIANT_HPP

#include "context.hpp"
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
        template <size_t N, Variant T>
        using variantT = typename std::variant_alternative<N, T>::type;

        template <Variant T>
        void setNthValue(T &var, size_t case_index, const LiftLowerContext &cx, uint32_t ptr)
        {
            constexpr size_t variantSize = std::variant_size<T>::value;

            if (case_index >= variantSize)
            {
                throw std::out_of_range("Invalid case_index for variant");
            }

            auto setter = [&]<size_t... Indices>(std::index_sequence<Indices...>)
            {
                ((case_index == Indices ? (var = load<variantT<Indices, T>>(cx, ptr), true) : false) || ...);
            };

            setter(std::make_index_sequence<variantSize>{});
        }

        template <Variant T>
        T load(const LiftLowerContext &cx, uint32_t ptr)
        {
            T retVal;
            uint32_t disc_size = ValTrait<typename ValTrait<T>::discriminant_type>::size;
            auto case_index = integer::load<typename ValTrait<T>::discriminant_type>(cx, ptr);
            ptr += disc_size;
            trap_if(cx, case_index >= std::variant_size_v<T>);
            ptr = align_to(ptr, ValTrait<T>::max_case_alignment);
            setNthValue(retVal, case_index, cx, ptr);
            return retVal;
        }

        struct StoreVisitor
        {
            LiftLowerContext &cx;
            uint32_t ptr;

            template <typename T>
            void operator()(T &&arg) const
            {
                store(cx, arg, ptr);
            }
        };

        template <Variant T>
        void store(LiftLowerContext &cx, const T &v, uint32_t ptr)
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
            LiftLowerContext &cx;
            template <typename T>
            WasmValVector operator()(T &&arg) const
            {
                return lower_flat(cx, std::forward<T>(arg));
            }
        };

        template <Variant T>
        WasmValVector lower_flat(LiftLowerContext &cx, const T &v)
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
        T lift_flat_helper(const LiftLowerContext &cx, const CoreValueIter &vi, int32_t case_index)
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
        inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
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

    //  Option ------------------------------------------------------------------
    template <Option T>
    inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr)
    {
        using V = variant_t<monostate, typename ValTrait<T>::inner_type>;
        if (v.has_value())
        {
            V variant_val = V(std::in_place_index<1>, v.value());
            variant::store<V>(cx, variant_val, ptr);
        }
        else
        {
            V variant_val = V(std::in_place_index<0>, monostate{});
            variant::store<V>(cx, variant_val, ptr);
        }
    }

    template <Option T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v)
    {
        using V = typename ValTrait<T>::variant_type;
        if (v.has_value())
        {
            V variant_val = V(std::in_place_index<1>, v.value());
            return variant::lower_flat<V>(cx, variant_val);
        }
        else
        {
            V variant_val = V(std::in_place_index<0>, monostate{});
            return variant::lower_flat<V>(cx, variant_val);
        }
    }

    template <Option T>
    inline T load(const LiftLowerContext &cx, uint32_t ptr)
    {
        T retVal;
        using V = variant_t<monostate, typename ValTrait<T>::inner_type>;
        auto v = variant::load<V>(cx, ptr);
        if (v.index() == 1)
        {
            retVal = std::get<1>(v);
        }
        return retVal;
    }

    template <Option T>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
    {
        T retVal;
        using V = variant_t<monostate, typename ValTrait<T>::inner_type>;
        auto v = variant::lift_flat<V>(cx, vi);
        if (v.index() == 1)
        {
            retVal = std::get<1>(v);
        }
        return retVal;
    }

    //  Variant ------------------------------------------------------------------
    template <Variant T>
    inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr)
    {
        variant::store(cx, v, ptr);
    }

    template <Variant T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v)
    {
        return variant::lower_flat(cx, v);
    }

    template <Variant T>
    inline T load(const LiftLowerContext &cx, uint32_t ptr)
    {
        return variant::load<T>(cx, ptr);
    }

    template <Variant T>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
    {
        return variant::lift_flat<T>(cx, vi);
    }
}

#endif
