#ifndef CMCPP_BOOL_HPP
#define CMCPP_BOOL_HPP

#include "context.hpp"
#include "integer.hpp"
#include "util.hpp"

namespace cmcpp
{
    //  Boolean  ------------------------------------------------------------------
    template <Boolean T>
    inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr)
    {
        uint8_t byte = v ? 1 : 0;
        integer::store<uint8_t>(cx, byte, ptr);
    }

    template <Boolean T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v)
    {
        using WasmValType = WasmValTypeTrait<ValTrait<T>::flat_types[0]>::type;
        return {static_cast<WasmValType>(v)};
    }

    template <Boolean T>
    inline T load(const LiftLowerContext &cx, uint32_t ptr)
    {
        return convert_int_to_bool(integer::load<uint8_t>(cx, ptr));
    }

    template <Boolean T>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
    {
        return convert_int_to_bool(vi.next<int32_t>());
    }
}

#endif // CMCPP_BOOL_HPP
