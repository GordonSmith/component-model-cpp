#ifndef CMCPP_FUNC_HPP
#define CMCPP_FUNC_HPP

#include "context.hpp"

namespace cmcpp
{

    namespace funcXXX
    {
        //     template <Flags T>
        //     int32_t pack_flags_into_int(const T &v)
        //     {
        //         return v.to_ulong();
        //     }

        //     template <Flags T>
        //     void store(LiftLowerContext &cx, const T &v, offset ptr)
        //     {
        //         auto i = pack_flags_into_int(v);
        //         std::memcpy(&cx.opts.memory[ptr], i, ValTrait<T>::size);
        //     }

        //     template <Flags T>
        //     WasmValVector lower_flat(LiftLowerContext &cx, const T &v)
        //     {
        //         return {pack_flags_into_int(v)};
        //     }

        //     template <Flags T>
        //     T unpack_flags_from_int(const uint32_t &buff)
        //     {
        //         return {buff};
        //     }

        //     template <Flags T>
        //     T load(const LiftLowerContext &cx, uint32_t ptr)
        //     {
        //         uint8_t buff[ValTrait<T>::size];
        //         std::memcpy(&buff, &cx.opts.memory[ptr], ValTrait<T>::size);
        //         return unpack_flags_from_int<T>(buff);
        //     }

        //     template <Flags T>
        //     T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
        //     {
        //         auto i = vi.next<int32_t>();
        //         uint8_t buff[ValTrait<T>::size];
        //         std::memcpy(&buff, &i, ValTrait<T>::size);
        //         return unpack_flags_from_int<T>(i);
        //     }
    }

    template <Func T>
    inline void flatten(LiftLowerContext &cx, const T &v, uint32_t ptr)
    {
        flags::store(cx, v, ptr);
    }

    // template <Flags T>
    // inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr)
    // {
    //     flags::store(cx, v, ptr);
    // }

    // template <Flags T>
    // inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v)
    // {
    //     return flags::lower_flat(cx, v);
    // }

    // template <Flags T>
    // inline T load(const LiftLowerContext &cx, uint32_t ptr)
    // {
    //     return flags::load<T>(cx, ptr);
    // }

    // template <Flags T>
    // inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
    // {
    //     return flags::lift_flat<T>(cx, vi);
    // }
}

#endif
