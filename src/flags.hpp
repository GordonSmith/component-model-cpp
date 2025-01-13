#ifndef CMCPP_FLAGS_HPP
#define CMCPP_FLAGS_HPP

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
            return v.to_ulong();
        }

        template <Flags T>
        void store(CallContext &cx, const T &v, offset ptr)
        {
            auto i = pack_flags_into_int(v);
            std::memcpy(&cx.memory[ptr], i, ValTrait<T>::size);
        }

        template <Flags T>
        WasmValVector lower_flat(CallContext &cx, const T &v)
        {
            return {pack_flags_into_int(v)};
        }

        template <Flags T>
        T unpack_flags_from_int(const uint32_t &buff)
        {
            return {buff};
        }

        template <Flags T>
        T load(const CallContext &cx, uint32_t ptr)
        {
            uint8_t buff[ValTrait<T>::size];
            std::memcpy(&buff, &cx.memory[ptr], ValTrait<T>::size);
            return unpack_flags_from_int<T>(buff);
        }

        template <Flags T>
        T lift_flat(const CallContext &cx, const WasmValVectorIterator &vi)
        {
            auto i = vi.next<int32_t>();
            uint8_t buff[ValTrait<T>::size];
            std::memcpy(&buff, &i, ValTrait<T>::size);
            return unpack_flags_from_int<T>(i);
        }
    }
}
#endif
