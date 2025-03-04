#ifndef CMCPP_LIFT_HPP
#define CMCPP_LIFT_HPP

#include "context.hpp"
#include "util.hpp"

namespace cmcpp
{
    template <Boolean T>
    inline T lift_flat(const CallContext &cx, const CoreValueIter &vi)
    {
        return convert_int_to_bool(vi.next<int32_t>());
    }

    template <Char T>
    inline T lift_flat(const CallContext &cx, const CoreValueIter &vi)
    {
        return convert_i32_to_char(cx, vi.next<int32_t>());
    }

    template <UnsignedInteger T>
    inline T lift_flat(const CallContext &cx, const CoreValueIter &vi);

    template <SignedInteger T>
    inline T lift_flat(const CallContext &cx, const CoreValueIter &vi);

    template <Float T>
    inline T lift_flat(const CallContext &cx, const CoreValueIter &vi);

    template <String T>
    inline T lift_flat(const CallContext &cx, const CoreValueIter &vi);

    template <List T>
    inline T lift_flat(const CallContext &cx, const CoreValueIter &vi);

    template <Flags T>
    inline T lift_flat(const CallContext &cx, const CoreValueIter &vi);

    template <Tuple T>
    inline T lift_flat(const CallContext &cx, const CoreValueIter &vi);

    template <Record T>
    inline T lift_flat(const CallContext &cx, const CoreValueIter &vi);

    template <Variant T>
    inline T lift_flat(const CallContext &cx, const CoreValueIter &vi);

}

#endif
