#ifndef CMCPP_LOWER_HPP
#define CMCPP_LOWER_HPP

#include "context.hpp"
#include "integer.hpp"
#include "float.hpp"
#include "string.hpp"
#include "list.hpp"
#include "flags.hpp"
#include "tuple.hpp"
#include "func.hpp"
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

    template <Tuple T>
    inline WasmValVector lower_heap_values(LiftLowerContext &cx, const T &vs)
    {
        using tuple_type = tuple_t<T>;
        tuple_type tuple_value = vs;
        auto ptr = cx.opts.realloc(0, 0, ValTrait<T>::alignment, ValTrait<T>::size);
        WasmValVector flat_vals = {ptr};
        trap_if(cx, ptr != align_to(ptr, ValTrait<tuple_type>::alignment));
        trap_if(cx, ptr + ValTrait<tuple_type>::size > cx.opts.memory.size());
        store(cx, vs, ptr);
        return flat_vals;
    }

    template <Tuple T>
    inline WasmValVector lower_flat_values(LiftLowerContext &cx, uint max_flat, const T &vs)
    {
        // cx.inst.may_leave=false;
        WasmValVector retVal = {};
        auto flat_types = ValTrait<T>::flat_types;
        if (flat_types.size() > max_flat)
        {
            retVal = lower_heap_values(cx, vs);
        }
        else
        {
            retVal = lower_flat(cx, vs);
        }
        // cx.inst.may_leave=true;
        return retVal;
    }
}

#endif
