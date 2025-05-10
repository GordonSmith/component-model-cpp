#ifndef CMCPP_INTEGER_HPP
#define CMCPP_INTEGER_HPP

#include "context.hpp"
#include "util.hpp"

namespace cmcpp
{
    namespace integer
    {
        template <typename T>
        void store(LiftLowerContext &cx, const T &v, offset ptr, uint32_t size = ValTrait<T>::size)
        {
            std::memcpy(&cx.opts.memory[ptr], &v, size);
        }

        template <typename T>
        WasmValVector lower_flat_signed(const T &v, uint32_t core_bits)
        {
            using WasmValType = WasmValTypeTrait<ValTrait<T>::flat_types[0]>::type;
            WasmValType retVal = v;
            return {v};
        }

        template <typename T>
        T load(const LiftLowerContext &cx, offset ptr, uint32_t size = ValTrait<T>::size)
        {
            T retVal;
            std::memcpy(&retVal, &cx.opts.memory[ptr], size);
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

    //  Boolean  ------------------------------------------------------------------
    template <Boolean T>
    inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr)
    {
        integer::store<T>(cx, v, ptr);
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

    //  Char  ------------------------------------------------------------------
    template <Char T>
    inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr)
    {
        integer::store<T>(cx, char_to_i32(cx, v), ptr);
    }

    template <Char T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v)
    {
        using WasmValType = WasmValTypeTrait<ValTrait<T>::flat_types[0]>::type;
        return {static_cast<WasmValType>(char_to_i32(cx, v))};
    }

    template <Char T>
    inline T load(const LiftLowerContext &cx, uint32_t ptr)
    {
        return convert_i32_to_char(cx, integer::load<uint32_t>(cx, ptr));
    }

    template <Char T>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
    {
        return convert_i32_to_char(cx, vi.next<int32_t>());
    }

    //  Integer  ------------------------------------------------------------------
    template <Integer T>
    inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr)
    {
        integer::store<T>(cx, v, ptr);
    }

    template <SignedInteger T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v)
    {
        using WasmValType = WasmValTypeTrait<ValTrait<T>::flat_types[0]>::type;
        return integer::lower_flat_signed(v, ValTrait<WasmValType>::size * 8);
    }

    template <UnsignedInteger T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v)
    {
        using WasmValType = WasmValTypeTrait<ValTrait<T>::flat_types[0]>::type;
        WasmValType fv = v;
        return {fv};
    }

    template <Integer T>
    inline T load(const LiftLowerContext &cx, uint32_t ptr)
    {
        return integer::load<T>(cx, ptr);
    }

    template <UnsignedInteger T>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
    {
        return integer::lift_flat_unsigned<T>(vi, ValTrait<T>::size * 8, 8);
    }

    template <SignedInteger T>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
    {
        return integer::lift_flat_signed<T>(vi, ValTrait<T>::size * 8, 8);
    }
}

#endif
