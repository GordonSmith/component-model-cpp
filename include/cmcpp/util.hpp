#ifndef CMCPP_UTIL_HPP
#define CMCPP_UTIL_HPP

#include "context.hpp"

namespace cmcpp
{
    const bool DETERMINISTIC_PROFILE = false;

    inline bool_t convert_int_to_bool(uint8_t i)
    {
        return i > 0;
    }

    inline char_t convert_i32_to_char(const LiftLowerContext &cx, int32_t i)
    {
        trap_if(cx, i >= 0x110000);
        trap_if(cx, 0xD800 <= i && i <= 0xDFFF);
        return i;
    }

    inline int32_t char_to_i32(const LiftLowerContext &cx, const char_t &v)
    {
        uint32_t retVal = v;
        trap_if(cx, retVal >= 0x110000);
        trap_if(cx, 0xD800 <= retVal && retVal <= 0xDFFF, "Invalid char value");
        return retVal;
    }

    inline int32_t wrap_i64_to_i32(int64_t x)
    {
        if (x < std::numeric_limits<int32_t>::lowest() || x > std::numeric_limits<int32_t>::max())
        {
            return std::numeric_limits<int32_t>::lowest();
        }
        return static_cast<int32_t>(x);
    }

    template <typename T>
    inline uint32_t checked_uint32(T value, const char *message = "value does not fit in uint32_t")
    {
        static_assert(std::is_integral_v<std::decay_t<T>> || std::is_enum_v<std::decay_t<T>>, "checked_uint32 expects an integral or enum type");

        using ValueType = std::decay_t<T>;
        if constexpr (std::is_signed_v<ValueType>)
        {
            if (value < 0)
            {
                throw std::overflow_error(message);
            }
        }

        auto wide = static_cast<uint64_t>(value);
        if (wide > std::numeric_limits<uint32_t>::max())
        {
            throw std::overflow_error(message);
        }
        return static_cast<uint32_t>(wide);
    }

    template <typename T>
    inline uint32_t checked_uint32(const LiftLowerContext &cx, T value, const char *message = "value does not fit in uint32_t")
    {
        static_assert(std::is_integral_v<std::decay_t<T>> || std::is_enum_v<std::decay_t<T>>, "checked_uint32 expects an integral or enum type");

        using ValueType = std::decay_t<T>;
        if constexpr (std::is_signed_v<ValueType>)
        {
            trap_if(cx, value < 0, message);
        }

        trap_if(cx, static_cast<uint64_t>(value) > std::numeric_limits<uint32_t>::max(), message);
        return static_cast<uint32_t>(value);
    }

    inline float32_t decode_i32_as_float(int32_t i)
    {
        return *reinterpret_cast<float32_t *>(&i);
    }

    inline float64_t decode_i64_as_float(int64_t i)
    {
        return *reinterpret_cast<float64_t *>(&i);
    }

    template <typename T>
    inline int32_t checked_int32(T value, const char *message = "value does not fit in int32_t")
    {
        static_assert(std::is_integral_v<std::decay_t<T>> || std::is_enum_v<std::decay_t<T>>, "checked_int32 expects an integral or enum type");

        using ValueType = std::decay_t<T>;
        constexpr auto min = static_cast<int64_t>(std::numeric_limits<int32_t>::min());
        constexpr auto max = static_cast<int64_t>(std::numeric_limits<int32_t>::max());

        int64_t wide;
        if constexpr (std::is_enum_v<ValueType>)
        {
            using Underlying = std::underlying_type_t<ValueType>;
            if constexpr (std::is_signed_v<Underlying>)
            {
                wide = static_cast<int64_t>(static_cast<Underlying>(value));
            }
            else
            {
                wide = static_cast<int64_t>(static_cast<uint64_t>(static_cast<Underlying>(value)));
            }
        }
        else if constexpr (std::is_signed_v<ValueType>)
        {
            wide = static_cast<int64_t>(value);
        }
        else
        {
            wide = static_cast<int64_t>(static_cast<uint64_t>(value));
        }

        if (wide < min || wide > max)
        {
            throw std::overflow_error(message);
        }

        return static_cast<int32_t>(wide);
    }

    template <typename T>
    inline int32_t checked_int32(const LiftLowerContext &cx, T value, const char *message = "value does not fit in int32_t")
    {
        static_assert(std::is_integral_v<std::decay_t<T>> || std::is_enum_v<std::decay_t<T>>, "checked_int32 expects an integral or enum type");

        using ValueType = std::decay_t<T>;
        constexpr auto min = static_cast<int64_t>(std::numeric_limits<int32_t>::min());
        constexpr auto max = static_cast<int64_t>(std::numeric_limits<int32_t>::max());

        int64_t wide;
        if constexpr (std::is_enum_v<ValueType>)
        {
            using Underlying = std::underlying_type_t<ValueType>;
            if constexpr (std::is_signed_v<Underlying>)
            {
                wide = static_cast<int64_t>(static_cast<Underlying>(value));
            }
            else
            {
                wide = static_cast<int64_t>(static_cast<uint64_t>(static_cast<Underlying>(value)));
            }
        }
        else if constexpr (std::is_signed_v<ValueType>)
        {
            wide = static_cast<int64_t>(value);
        }
        else
        {
            wide = static_cast<int64_t>(static_cast<uint64_t>(value));
        }

        trap_if(cx, wide < min || wide > max, message);
        return static_cast<int32_t>(wide);
    }

    class CoreValueIter
    {
        mutable WasmValVector::const_iterator it;
        WasmValVector::const_iterator end;

    public:
        CoreValueIter(const WasmValVector &v) : it(v.begin()), end(v.end())
        {
        }

        template <FlatValue T>
        T next() const
        {
            return std::get<T>(next(WasmValTrait<T>::type));
        }
        virtual WasmVal next(const WasmValType &t) const
        {
            assert(it != end);
            return *it++;
        }
        bool done() const
        {
            return !(it != end);
        }
    };

    class CoerceValueIter : public CoreValueIter
    {
        const CoreValueIter &vi;
        WasmValTypeVector &flat_types;

    public:
        CoerceValueIter(const CoreValueIter &vi, WasmValTypeVector &flat_types) : CoreValueIter({}), vi(vi), flat_types(flat_types)
        {
        }

        virtual WasmVal next(const WasmValType &want) const override
        {
            auto have = flat_types.front();
            flat_types.erase(flat_types.begin());
            auto x = vi.next(have);
            if (have == WasmValType::i32 && want == WasmValType::f32)
            {
                return decode_i32_as_float(std::get<int32_t>(x));
            }
            else if (have == WasmValType::i64 && want == WasmValType::i32)
            {
                return wrap_i64_to_i32(std::get<int64_t>(x));
            }
            else if (have == WasmValType::i64 && want == WasmValType::f32)
            {
                return decode_i32_as_float(wrap_i64_to_i32(std::get<int64_t>(x)));
            }
            else if (have == WasmValType::i64 && want == WasmValType::f64)
            {
                return decode_i64_as_float(std::get<int64_t>(x));
            }
            else
            {
                assert(have == want);
                return x;
            }
        }
    };

}

#endif
