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
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v);

    template <Char T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v);

    template <UnsignedInteger T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v);

    template <SignedInteger T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v);

    template <Float T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v);

    template <String T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v);

    template <Flags T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v);

    template <List T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v);

    template <Tuple T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v);

    template <Record T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v);

    template <Variant T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v);

    template <Option T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v);

    template <Field... Ts>
    inline WasmValVector lower_heap_values(LiftLowerContext &cx, uint32_t *out_param, Ts &&...vs)
    {
        using tuple_type = tuple_t<Ts...>;
        tuple_type tuple_value = {std::forward<Ts>(vs)...};
        uint32_t ptr;
        WasmValVector flat_vals = {};
        if (out_param == nullptr)
        {
            ptr = cx.opts.realloc(0, 0, ValTrait<tuple_type>::alignment, ValTrait<tuple_type>::size);
            flat_vals = {static_cast<int32_t>(ptr)};
        }
        else
        {
            ptr = *out_param;
            flat_vals = {};
        }
        trap_if(cx, ptr != align_to(ptr, ValTrait<tuple_type>::alignment));
        trap_if(cx, ptr + ValTrait<tuple_type>::size > cx.opts.memory.size());
        store<tuple_type>(cx, tuple_value, ptr);
        return flat_vals;
    }

    template <Field... Ts>
    inline WasmValVector lower_flat_values(LiftLowerContext &cx, uint32_t max_flat, uint32_t *out_param, Ts &&...vs)
    {
        WasmValVector retVal = {};
        // cx.inst.may_leave=false;
        constexpr auto flat_types = ValTrait<tuple_t<Ts...>>::flat_types;
        if (flat_types.size() > max_flat)
        {
            retVal = lower_heap_values(cx, out_param, std::forward<Ts>(vs)...);
        }
        else
        {
            auto lower_v = [&](auto &&v)
            {
                auto flat = lower_flat(cx, v);
                retVal.insert(retVal.end(), flat.begin(), flat.end());
            };
            (lower_v(vs), ...);
            return retVal;
        }
        // cx.inst.may_leave=true;
        return retVal;
    }

}

#endif
