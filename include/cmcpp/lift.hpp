#ifndef CMCPP_LIFT_HPP
#define CMCPP_LIFT_HPP

#include "context.hpp"
#include "util.hpp"
#include "load.hpp"

namespace cmcpp
{
    template <Boolean T>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
    {
        return convert_int_to_bool(vi.next<int32_t>());
    }

    template <Char T>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
    {
        return convert_i32_to_char(cx, vi.next<int32_t>());
    }

    template <UnsignedInteger T>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi);

    template <SignedInteger T>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi);

    template <Float T>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi);

    template <String T>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi);

    template <List T>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi);

    template <Flags T>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi);

    template <Tuple T>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi);

    template <Record T>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi);

    template <Variant T>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi);

    template <Option T>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi);

    template <Tuple T>
    inline T lift_heap_values(const LiftLowerContext &cx, const CoreValueIter &vi)
    {
        uint32_t ptr = vi.next<int32_t>();
        using tuple_type = typename std::tuple_element<0, typename ValTrait<T>::inner_type>::type;
        trap_if(cx, ptr != align_to(ptr, ValTrait<tuple_type>::alignment));
        trap_if(cx, ptr + ValTrait<tuple_type>::size > cx.opts.memory.size());
        auto retVal = load<tuple_type>(cx, ptr);
        return retVal;
    }

    template <Tuple T>
    inline T lift_flat_values(const LiftLowerContext &cx, uint max_flat, const CoreValueIter &vi)
    {
        auto flat_types = ValTrait<T>::flat_types;
        if (flat_types.size() > max_flat)
        {
            return lift_heap_values<T>(cx, vi);
        }
        return lift_flat<T>(cx, vi);
    }
}

#endif
