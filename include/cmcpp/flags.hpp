#ifndef CMCPP_FLAGS_HPP
#define CMCPP_FLAGS_HPP

#include <cstring>

#include "context.hpp"
#include "integer.hpp"
#include "util.hpp"

namespace cmcpp
{
    namespace flags
    {
        template <Flags T>
        int32_t pack_flags_into_int(const T &v)
        {
            return static_cast<int32_t>(v.to_ulong());
        }

        template <Flags T>
        void store(LiftLowerContext &cx, const T &v, offset ptr)
        {
            auto i = pack_flags_into_int(v);
            std::memcpy(&cx.opts.memory[ptr], &i, ValTrait<T>::size);
        }

        template <Flags T>
        WasmValVector lower_flat(LiftLowerContext &cx, const T &v)
        {
            return {pack_flags_into_int(v)};
        }

        template <Flags T>
        T unpack_flags_from_int(const uint32_t &buff)
        {
            T value{};
            using bitset_type = typename ValTrait<T>::inner_type;
            static_cast<bitset_type &>(value) = bitset_type(static_cast<unsigned long long>(buff));
            return value;
        }

        template <Flags T>
        T load(const LiftLowerContext &cx, uint32_t ptr)
        {
            uint32_t raw = 0;
            std::memcpy(&raw, &cx.opts.memory[ptr], ValTrait<T>::size);
            return unpack_flags_from_int<T>(raw);
        }

        template <Flags T>
        T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
        {
            auto i = vi.next<int32_t>();
            return unpack_flags_from_int<T>(static_cast<uint32_t>(i));
        }
    }

    template <Flags T>
    inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr)
    {
        flags::store(cx, v, ptr);
    }

    template <Flags T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v)
    {
        return flags::lower_flat(cx, v);
    }

    template <Flags T>
    inline T load(const LiftLowerContext &cx, uint32_t ptr)
    {
        return flags::load<T>(cx, ptr);
    }

    template <Flags T>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
    {
        return flags::lift_flat<T>(cx, vi);
    }

}
#endif
