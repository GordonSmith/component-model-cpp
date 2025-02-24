#ifndef CMCPP_LIFT_HPP
#define CMCPP_LIFT_HPP

#include "context.hpp"
#include "integer.hpp"
#include "float.hpp"
#include "string.hpp"
#include "list.hpp"
#include "flags.hpp"
#include "record.hpp"
#include "variant.hpp"
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
    inline T lift_flat(const CallContext &cx, const CoreValueIter &vi)
    {
        return integer::lift_flat_unsigned<T>(vi, ValTrait<T>::size * 8, 8);
    }

    template <SignedInteger T>
    inline T lift_flat(const CallContext &cx, const CoreValueIter &vi)
    {
        return integer::lift_flat_signed<T>(vi, ValTrait<T>::size * 8, 8);
    }

    template <Float T>
    inline T lift_flat(const CallContext &cx, const CoreValueIter &vi)
    {
        using WasmValType = ValTrait<T>::flat_type;
        return float_::canonicalize_nan<T>(std::get<WasmValType>(vi.next(ValTrait<T>::flat_types[0])));
    }

    template <String T>
    inline T lift_flat(const CallContext &cx, const CoreValueIter &vi)
    {
        return string::lift_flat<T>(cx, vi);
    }

    template <List T>
    inline T lift_flat(const CallContext &cx, const CoreValueIter &vi)
    {
        return list::lift_flat<typename ValTrait<T>::inner_type>(cx, vi);
    }

    template <Flags T>
    inline T lift_flat(const CallContext &cx, const CoreValueIter &vi)
    {
        return flags::lift_flat<T>(cx, vi);
    }

    template <Record T>
    T lift_flat(const CallContext &cx, const CoreValueIter &vi)
    {
        auto x = record::lift_flat_record<T>(cx, vi);
        return x;
    }

    template <Variant T>
    T lift_flat(const CallContext &cx, const CoreValueIter &vi)
    {
        return variant::lift_flat<T>(cx, vi);
    }
}

#endif
