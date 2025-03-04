#ifndef CMCPP_INTEGER_HPP
#define CMCPP_INTEGER_HPP

#include "context.hpp"
#include "util.hpp"

#include <cstring>
#include <iostream>
#include <cassert>

namespace cmcpp
{
    using offset = uint32_t;

    namespace integer
    {
        template <typename T>
        void store(CallContext &cx, const T &v, offset ptr, uint32_t size = ValTrait<T>::size)
        {
            std::memcpy(&cx.memory[ptr], &v, size);
        }

        template <typename T>
        WasmValVector lower_flat_signed(const T &v, uint32_t core_bits)
        {
            using WasmValType = WasmValTypeTrait<ValTrait<T>::flat_types[0]>::type;
            WasmValType retVal = v;
            return {v};
        }

        template <typename T>
        T load(const CallContext &cx, offset ptr, uint32_t size = ValTrait<T>::size)
        {
            T retVal;
            std::memcpy(&retVal, &cx.memory[ptr], size);
            return retVal;
        }

        template <typename T>
        T lift_flat_unsigned(const CoreValueIter &vi, uint32_t core_width, uint32_t t_width)
        {
            using WasmValType = WasmValTypeTrait<ValTrait<T>::flat_types[0]>::type;
            auto retVal = std::get<WasmValType>(vi.next(ValTrait<T>::flat_types[0]));
            assert(ValTrait<WasmValType>::LOW_VALUE <= retVal && retVal < ValTrait<WasmValType>::HIGH_VALUE);
            return retVal;
        }

        template <typename T>
        T lift_flat_signed(const CoreValueIter &vi, uint32_t core_width, uint32_t t_width)
        {
            using WasmValType = WasmValTypeTrait<ValTrait<T>::flat_types[0]>::type;
            auto retVal = static_cast<T>(std::get<WasmValType>(vi.next(ValTrait<T>::flat_types[0])));
            assert(ValTrait<T>::LOW_VALUE <= retVal && retVal <= ValTrait<T>::HIGH_VALUE);
            return retVal;
        }
    }

    template <Boolean T>
    inline void store(CallContext &cx, const T &v, uint32_t ptr)
    {
        integer::store<T>(cx, v, ptr);
    }

    template <Char T>
    inline void store(CallContext &cx, const T &v, uint32_t ptr)
    {
        integer::store<T>(cx, char_to_i32(cx, v), ptr);
    }

    template <Integer T>
    inline void store(CallContext &cx, const T &v, uint32_t ptr)
    {
        integer::store<T>(cx, v, ptr);
    }

    template <Boolean T>
    inline T load(const CallContext &cx, uint32_t ptr)
    {
        return convert_int_to_bool(integer::load<uint8_t>(cx, ptr));
    }

    template <Char T>
    inline T load(const CallContext &cx, uint32_t ptr)
    {
        return convert_i32_to_char(cx, integer::load<uint32_t>(cx, ptr));
    }

    template <Integer T>
    inline T load(const CallContext &cx, uint32_t ptr)
    {
        return integer::load<T>(cx, ptr);
    }
}

#endif
