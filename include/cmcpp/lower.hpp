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
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v)
    {
        using WasmValType = WasmValTypeTrait<ValTrait<T>::flat_types[0]>::type;
        return {static_cast<WasmValType>(v)};
    }

    template <Char T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v)
    {
        using WasmValType = WasmValTypeTrait<ValTrait<T>::flat_types[0]>::type;
        return {static_cast<WasmValType>(char_to_i32(cx, v))};
    }

    template <UnsignedInteger T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v)
    {
        using WasmValType = WasmValTypeTrait<ValTrait<T>::flat_types[0]>::type;
        WasmValType fv = v;
        return {fv};
    }

    template <SignedInteger T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v);

    template <Float T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v);

    template <String T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v);

    template <List T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v);

    template <Flags T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v);

    template <Tuple T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v);

    template <Record T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v);

    template <Variant T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v);

    template <Option T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v);
}

#endif
