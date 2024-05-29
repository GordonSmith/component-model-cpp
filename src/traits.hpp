#ifndef TRAITS_HPP
#define TRAITS_HPP

#include "context.hpp"

#include <optional>
#include <variant>

namespace cmcpp
{

    using float32_t = float;
    using float64_t = double;

    enum class ValType : uint8_t
    {
        Unknown,
        Bool,
        S8,
        U8,
        S16,
        U16,
        S32,
        U32,
        S64,
        U64,
        F32,
        F64,
        Char,
        String,
        List,
        Field,
        Record,
        Tuple,
        Case,
        Variant,
        Enum,
        Option,
        Result,
        Flags,
        Own,
        Borrow
    };

    template <typename T>
    struct ValTrait
    {
        static ValType type()
        {
            throw std::runtime_error(typeid(T).name());
        }
    };

    template <>
    struct ValTrait<bool>
    {
        static ValType type()
        {
            return ValType::Bool;
        }
    };

    template <>
    struct ValTrait<int8_t>
    {
        static ValType type()
        {
            return ValType::S8;
        }
    };

    template <>
    struct ValTrait<uint8_t>
    {
        static ValType type()
        {
            return ValType::U8;
        }
    };

    template <>
    struct ValTrait<int16_t>
    {
        static ValType type()
        {
            return ValType::S16;
        }
    };

    template <>
    struct ValTrait<uint16_t>
    {
        static ValType type()
        {
            return ValType::U16;
        }
    };

    template <>
    struct ValTrait<int32_t>
    {
        static ValType type()
        {
            return ValType::S32;
        }
    };

    template <>
    struct ValTrait<uint32_t>
    {
        static ValType type()
        {
            return ValType::U32;
        }
    };

    template <>
    struct ValTrait<int64_t>
    {
        static ValType type()
        {
            return ValType::S64;
        }
    };

    template <>
    struct ValTrait<uint64_t>
    {
        static ValType type()
        {
            return ValType::U64;
        }
    };

    template <>
    struct ValTrait<float>
    {
        static ValType type()
        {
            return ValType::F32;
        }
    };

    template <>
    struct ValTrait<double>
    {
        static ValType type()
        {
            return ValType::F64;
        }
    };

    template <>
    struct ValTrait<char>
    {
        static ValType type()
        {
            return ValType::Char;
        }
    };

    template <typename T>
    struct ValTrait<std::optional<T>>
    {
        static ValType type()
        {
            return ValTrait<T>::type();
        }
    };

    template <typename T>
    ValType type(const T &v)
    {
        return ValTrait<T>::type();
    }

    using Val = std::variant<bool, int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float32_t, float64_t, char>;

    using Bool = std::optional<bool>;
    using S8 = std::optional<int8_t>;
    using U8 = std::optional<uint8_t>;
    using S16 = std::optional<int16_t>;
    using U16 = std::optional<uint16_t>;
    using S32 = std::optional<int32_t>;
    using U32 = std::optional<uint32_t>;
    using S64 = std::optional<int64_t>;
    using U64 = std::optional<uint64_t>;
    using F32 = std::optional<float32_t>;
    using F64 = std::optional<float64_t>;
    using Char = std::optional<char>;
    using Ref = std::variant<Bool, S8, U8, S16, U16, S32, U32, S64, U64, F32, F64, Char>;

    template <typename T>
    struct WasmValTrait
    {
        static const char *type()
        {
            throw std::runtime_error(typeid(T).name());
        }
    };

    const char *const i32 = "i32";
    const char *const i64 = "i64";
    const char *const f32 = "f32";
    const char *const f64 = "f64";

    template <>
    struct WasmValTrait<int32_t>
    {
        static const char *type()
        {
            return i32;
        }
    };

    template <>
    struct WasmValTrait<int64_t>
    {
        static const char *type()
        {
            return i64;
        }
    };

    template <>
    struct WasmValTrait<float32_t>
    {
        static const char *type()
        {
            return f32;
        }
    };

    template <>
    struct WasmValTrait<float64_t>
    {
        static const char *type()
        {
            return f64;
        }
    };

    using WasmVal = std::variant<int32_t, int64_t, float32_t, float64_t>;
    using WasmRef = std::variant<S32, S64, F32, F64>;
}

#endif
