#ifndef CMCPP_FIXED_LIST_HPP
#define CMCPP_FIXED_LIST_HPP

#include "context.hpp"
#include "lift.hpp"
#include "load.hpp"
#include "lower.hpp"
#include "store.hpp"

namespace cmcpp
{
    template <FixedList T>
    inline void store(LiftLowerContext &cx, const T &values, uint32_t ptr)
    {
        using Element = typename ValTrait<T>::inner_type;
        for (size_t i = 0; i < values.size(); ++i)
        {
            cmcpp::store(cx, values[i], ptr + i * ValTrait<Element>::size);
        }
    }

    template <FixedList T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &values)
    {
        WasmValVector flat;
        flat.reserve(ValTrait<T>::flat_types_len);
        for (const auto &value : values)
        {
            auto element_flat = cmcpp::lower_flat(cx, value);
            flat.insert(flat.end(), element_flat.begin(), element_flat.end());
        }
        return flat;
    }

    template <FixedList T>
    inline T load(const LiftLowerContext &cx, uint32_t ptr)
    {
        using Element = typename ValTrait<T>::inner_type;
        T values{};
        for (size_t i = 0; i < values.size(); ++i)
        {
            values[i] = cmcpp::load<Element>(cx, ptr + i * ValTrait<Element>::size);
        }
        return values;
    }

    template <FixedList T>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
    {
        using Element = typename ValTrait<T>::inner_type;
        T values{};
        for (auto &value : values)
        {
            value = cmcpp::lift_flat<Element>(cx, vi);
        }
        return values;
    }
}

#endif