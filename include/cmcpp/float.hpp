#ifndef CMCPP_FLOAT_HPP
#define CMCPP_FLOAT_HPP

#include "context.hpp"
#include "integer.hpp"

namespace cmcpp
{
    namespace float_
    {
        inline int32_t encode_float_as_i32(float32_t f)

        {
            return *reinterpret_cast<int32_t *>(&f);
        }

        inline int64_t encode_float_as_i64(float64_t f)
        {
            return *reinterpret_cast<int64_t *>(&f);
        }

        inline float32_t core_f32_reinterpret_i32(int32_t i)
        {
            float f;
            std::memcpy(&f, &i, sizeof f);
            return f;
        }

        inline float64_t core_f64_reinterpret_i64(int64_t i)
        {
            float64_t f;
            std::memcpy(&f, &i, sizeof f);
            return f;
        }

        template <Float T>
        T canonicalize_nan(T f)
        {
            if (std::isnan(f))
            {
                // Use canonical NaN bit patterns per Component Model spec
                // Ref: ref/component-model/design/mvp/canonical-abi/definitions.py
                if constexpr (std::is_same_v<T, float32_t>)
                {
                    constexpr uint32_t CANONICAL_FLOAT32_NAN = 0x7fc00000;
                    return core_f32_reinterpret_i32(CANONICAL_FLOAT32_NAN);
                }
                else if constexpr (std::is_same_v<T, float64_t>)
                {
                    constexpr uint64_t CANONICAL_FLOAT64_NAN = 0x7ff8000000000000;
                    return core_f64_reinterpret_i64(CANONICAL_FLOAT64_NAN);
                }
            }
            return f;
        }

        template <typename T>
        inline void store(LiftLowerContext &cx, const T &v, offset ptrnbytes)
        {
            cx.trap("store of unsupported type");
            throw std::runtime_error("trap not terminating execution");
        }

        template <>
        inline void store<float32_t>(LiftLowerContext &cx, const float32_t &v, offset ptr)
        {
            integer::store(cx, encode_float_as_i32(v), ptr);
        }

        template <>
        inline void store<float64_t>(LiftLowerContext &cx, const float64_t &v, offset ptr)
        {
            integer::store(cx, encode_float_as_i64(v), ptr);
        }

        template <Float T>
        WasmValVector lower_flat(T f)
        {
            return {canonicalize_nan(f)};
        }

        template <typename T>
        T load(const LiftLowerContext &cx, offset ptr)
        {
            cx.trap("load of unsupported type");
            throw std::runtime_error("trap not terminating execution");
        }

        template <>
        inline float32_t load<float32_t>(const LiftLowerContext &cx, offset ptr)
        {
            return canonicalize_nan(decode_i32_as_float(integer::load<int32_t>(cx, ptr)));
        }

        template <>
        inline float64_t load<float64_t>(const LiftLowerContext &cx, offset ptr)
        {
            return canonicalize_nan(decode_i64_as_float(integer::load<int64_t>(cx, ptr)));
        }
    }

    template <Float T>
    inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr)
    {
        float_::store<T>(cx, v, ptr);
    }

    template <Float T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v)
    {
        return {float_::lower_flat<T>(v)};
    }

    template <Float T>
    inline T load(const LiftLowerContext &cx, uint32_t ptr)
    {
        return float_::load<T>(cx, ptr);
    }

    template <Float T>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
    {
        using WasmValType = WasmValTypeTrait<ValTrait<T>::flat_types[0]>::type;
        return float_::canonicalize_nan<T>(std::get<WasmValType>(vi.next(ValTrait<T>::flat_types[0])));
    }
}

#endif
