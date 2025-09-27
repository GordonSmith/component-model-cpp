#ifndef CMCPP_LIFT_HPP
#define CMCPP_LIFT_HPP

#include "context.hpp"
#include "util.hpp"
#include "load.hpp"

namespace cmcpp
{
    template <Boolean T>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi);

    template <Char T>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi);

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

    template <Field T>
    inline T lift_heap_values(const LiftLowerContext &cx, const CoreValueIter &vi)
    {
        uint32_t ptr = vi.next<int32_t>();
        trap_if(cx, ptr != align_to(ptr, ValTrait<T>::alignment));
        trap_if(cx, ptr + ValTrait<T>::size > cx.opts.memory.size());
        return load<T>(cx, ptr);
    }

    template <Field T>
    inline T lift_flat_values(const LiftLowerContext &cx, uint32_t max_flat, const CoreValueIter &vi)
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
