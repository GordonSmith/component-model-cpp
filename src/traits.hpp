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
        Float32,
        Float64,
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
        // Flags,
        // Own,
        // Borrow
    };

    enum class WasmValType : uint8_t
    {
        I32,
        I64,
        F32,
        F64
    };

    template <typename T>
    struct TypeNameTrait
    {
        static ValType name()
        {
            throw std::runtime_error("Unsupported Type");
        }
    };

    template <>
    struct TypeNameTrait<bool>
    {
        static ValType name()
        {
            return ValType::Bool;
        }
    };

    template <>
    struct TypeNameTrait<int8_t>
    {
        static ValType name()
        {
            return ValType::S8;
        }
    };

    template <>
    struct TypeNameTrait<uint8_t>
    {
        static ValType name()
        {
            return ValType::U8;
        }
    };

    template <>
    struct TypeNameTrait<int16_t>
    {
        static ValType name()
        {
            return ValType::S16;
        }
    };

    template <>
    struct TypeNameTrait<uint16_t>
    {
        static ValType name()
        {
            return ValType::U16;
        }
    };

    template <>
    struct TypeNameTrait<int32_t>
    {
        static ValType name()
        {
            return ValType::S32;
        }
    };

    template <>
    struct TypeNameTrait<uint32_t>
    {
        static ValType name()
        {
            return ValType::U32;
        }
    };

    template <>
    struct TypeNameTrait<int64_t>
    {
        static ValType name()
        {
            return ValType::S64;
        }
    };

    template <>
    struct TypeNameTrait<uint64_t>
    {
        static ValType name()
        {
            return ValType::U64;
        }
    };

    template <>
    struct TypeNameTrait<float>
    {
        static ValType name()
        {
            return ValType::Float32;
        }
    };

    template <>
    struct TypeNameTrait<double>
    {
        static ValType name()
        {
            return ValType::Float64;
        }
    };

    template <>
    struct TypeNameTrait<char>
    {
        static ValType name()
        {
            return ValType::Char;
        }
    };

    template <>
    struct TypeNameTrait<std::string>
    {
        static ValType name()
        {
            return ValType::String;
        }
    };

    template <typename LT>
    struct TypeNameTrait<std::vector<LT>>
    {
        static std::pair<ValType, ValType> name()
        {
            return {ValType::List, TypeNameTrait<LT>::name()};
        }
    };

    // template <typename T>
    // struct Val
    // {
    //     T value;

    //     Val(const T &val) : value(val) {}

    //     ValType kind() const
    //     {
    //         return TypeNameTrait<T>::name();
    //     }
    // };

    template <typename T>
    struct WasmTypeNameTrait
    {
        static WasmValType name()
        {
            throw std::runtime_error("Unsupported Type");
        }
    };

    template <>
    struct WasmTypeNameTrait<int32_t>
    {
        static WasmValType name()
        {
            return WasmValType::I32;
        }
    };

    template <>
    struct WasmTypeNameTrait<int64_t>
    {
        static WasmValType name()
        {
            return WasmValType::I64;
        }
    };

    template <>
    struct WasmTypeNameTrait<float32_t>
    {
        static WasmValType name()
        {
            return WasmValType::F32;
        }
    };

    template <>
    struct WasmTypeNameTrait<float64_t>
    {
        static WasmValType name()
        {
            return WasmValType::F64;
        }
    };

    // template <typename T>
    // struct WasmVal
    // {
    //     T value;

    //     WasmVal(const T &val) : value(val) {}

    //     WasmValType kind() const
    //     {
    //         return WasmTypeNameTrait<T>::name();
    //     }
    // };

}

#endif
