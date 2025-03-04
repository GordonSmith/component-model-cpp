#ifndef CMCPP_LOWER_HPP
#define CMCPP_LOWER_HPP

#include "context.hpp"
#include "integer.hpp"
#include "float.hpp"
#include "string.hpp"
#include "list.hpp"
#include "flags.hpp"
#include "tuple.hpp"
#include "util.hpp"

#include <tuple>
#include <cassert>

namespace cmcpp
{
    template <Boolean T>
    inline WasmValVector lower_flat(CallContext &cx, const T &v)
    {
        using WasmValType = WasmValTypeTrait<ValTrait<T>::flat_types[0]>::type;
        return {static_cast<WasmValType>(v)};
    }

    template <Char T>
    inline WasmValVector lower_flat(CallContext &cx, const T &v)
    {
        using WasmValType = WasmValTypeTrait<ValTrait<T>::flat_types[0]>::type;
        return {static_cast<WasmValType>(char_to_i32(cx, v))};
    }

    template <UnsignedInteger T>
    inline WasmValVector lower_flat(CallContext &cx, const T &v)
    {
        using WasmValType = WasmValTypeTrait<ValTrait<T>::flat_types[0]>::type;
        WasmValType fv = v;
        return {fv};
    }

    template <SignedInteger T>
    inline WasmValVector lower_flat(CallContext &cx, const T &v)
    {
        using WasmValType = WasmValTypeTrait<ValTrait<T>::flat_types[0]>::type;
        return integer::lower_flat_signed(v, ValTrait<WasmValType>::size * 8);
    }

    template <Float T>
    inline WasmValVector lower_flat(CallContext &cx, const T &v)
    {
        return {float_::lower_flat<T>(v)};
    }

    template <String T>
    inline WasmValVector lower_flat(CallContext &cx, const T &v)
    {
        return string::lower_flat<T>(cx, v);
    }

    template <List T>
    inline WasmValVector lower_flat(CallContext &cx, const T &v)
    {
        return list::lower_flat_list(cx, v);
    }

    template <Flags T>
    inline WasmValVector lower_flat(CallContext &cx, const T &v)
    {
        return flags::lower_flat(cx, v);
    }

    template <Tuple T>
    inline WasmValVector lower_flat(CallContext &cx, const T &v)
    {
        return tuple::lower_flat_tuple(cx, v);
    }

    template <Record T>
    inline WasmValVector lower_flat(CallContext &cx, const T &v)
    {
        return tuple::lower_flat_tuple(cx, boost::pfr::structure_to_tuple((typename ValTrait<T>::inner_type)v));
    }

    template <Variant T>
    inline WasmValVector lower_flat(CallContext &cx, const T &v)
    {
        return variant::lower_flat_variant(cx, v);
    }

}

#endif
