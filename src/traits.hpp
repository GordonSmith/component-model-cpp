#ifndef TRAITS_HPP
#define TRAITS_HPP

#include "context.hpp"

#include <optional>
#include <variant>
#include <map>

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

    // template <>
    // struct ValTrait<std::string>
    // {
    //     static ValType type()
    //     {
    //         return ValType::String;
    //     }
    // };
    // template <typename LT>
    // struct ValTrait<std::vector<LT>>
    // {
    //     static ValType type()
    //     {
    //         return ValType::List;
    //     }

    //     static ValType ltype()
    //     {
    //         return ValTrait<LT>::type();
    //     }
    // };

    // // template <typename RT1, typename MT2>
    // // struct ValTrait<std::map<std::string, MT2>>
    // // {
    // //     static ValType type()
    // //     {
    // //         return ValType::Record;
    // //     }

    // //     static std::pair<ValType, ValType> rtype()
    // //     {
    // //         return {ValTrait<MT1>::type(), ValTrait<MT2>::type()};
    // //     }
    // // };

    // template <typename T>
    // struct HostVal
    // {
    //     T value;

    //     HostVal(const T &val) : value(val) {}

    //     ValType type() const
    //     {
    //         return ValTrait<T>::type();
    //     }
    // };

    // enum class WasmValType : uint8_t
    // {
    //     I32,
    //     I64,
    //     F32,
    //     F64
    // };
    // using WasmValVariant = std::variant<int32_t, int64_t, float32_t, float64_t>;

    // template <typename T>
    // struct WasmValTrait
    // {
    //     static WasmValType name()
    //     {
    //         throw std::runtime_error("Unsupported Type");
    //     }
    // };

    // template <>
    // struct WasmValTrait<int32_t>
    // {
    //     static WasmValType name()
    //     {
    //         return WasmValType::I32;
    //     }

    //     static int32_t value(WasmValVariant v)
    //     {
    //         return std::get<int32_t>(v);
    //     }
    // };

    // template <>
    // struct WasmValTrait<int64_t>
    // {
    //     static WasmValType name()
    //     {
    //         return WasmValType::I64;
    //     }

    //     static int64_t value(WasmValVariant v)
    //     {

    //         return std::get<int64_t>(v);
    //     }
    // };

    // template <>
    // struct WasmValTrait<float32_t>
    // {
    //     static WasmValType name()
    //     {
    //         return WasmValType::F32;
    //     }

    //     static float32_t value(WasmValVariant v)
    //     {
    //         return std::get<float32_t>(v);
    //     }
    // };

    // template <>
    // struct WasmValTrait<float64_t>
    // {
    //     static WasmValType name()
    //     {
    //         return WasmValType::F64;
    //     }

    //     static float64_t value(WasmValVariant v)
    //     {
    //         return std::get<float64_t>(v);
    //     }
    // };

    // // template <typename T>
    // // struct WasmVal
    // // {
    // //     T value;

    // //     WasmVal(const T &val) : value(val) {}

    // //     WasmValType kind() const
    // //     {
    // //         return WasmValTrait<T>::name();
    // //     }
    // // };
}

#endif
