#ifndef TRAITS_HPP
#define TRAITS_HPP

#include "context.hpp"

#include <optional>
#include <variant>
#include <stdexcept>

namespace cmcpp
{

    using float32_t = float;
    using float64_t = double;

    enum class ValType : uint8_t
    {
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
    struct ValTrait<wchar_t>
    {
        static ValType type()
        {
            return ValType::Char;
        }
    };

    class string_t;
    using string_ptr = std::shared_ptr<string_t>;
    template <>
    struct ValTrait<string_ptr>
    {
        static ValType type() { return ValType::String; }
    };

    class list_t;
    using list_ptr = std::shared_ptr<list_t>;
    template <>
    struct ValTrait<list_ptr>
    {
        static ValType type() { return ValType::List; }
    };

    class field_t;
    using field_ptr = std::shared_ptr<field_t>;
    template <>
    struct ValTrait<field_ptr>
    {
        static ValType type() { return ValType::Field; }
    };

    class record_t;
    using record_ptr = std::shared_ptr<record_t>;
    template <>
    struct ValTrait<record_ptr>
    {
        static ValType type() { return ValType::Record; }
    };

    class tuple_t;
    using tuple_ptr = std::shared_ptr<tuple_t>;
    template <>
    struct ValTrait<tuple_ptr>
    {
        static ValType type() { return ValType::Tuple; }
    };

    class case_t;
    using case_ptr = std::shared_ptr<case_t>;
    template <>
    struct ValTrait<case_ptr>
    {
        static ValType type() { return ValType::Case; }
    };

    class variant_t;
    using variant_ptr = std::shared_ptr<variant_t>;
    template <>
    struct ValTrait<variant_ptr>
    {
        static ValType type() { return ValType::Variant; }
    };

    class enum_t;
    using enum_ptr = std::shared_ptr<enum_t>;
    template <>
    struct ValTrait<enum_ptr>
    {
        static ValType type() { return ValType::Enum; }
    };

    class option_t;
    using option_ptr = std::shared_ptr<option_t>;
    template <>
    struct ValTrait<option_ptr>
    {
        static ValType type() { return ValType::Option; }
    };

    class result_t;
    using result_ptr = std::shared_ptr<result_t>;
    template <>
    struct ValTrait<result_ptr>
    {
        static ValType type() { return ValType::Result; }
    };

    class flags_t;
    using flags_ptr = std::shared_ptr<flags_t>;
    template <>
    struct ValTrait<flags_ptr>
    {
        static ValType type() { return ValType::Flags; }
    };

    //  --------------------------------------------------------------------
    template <typename T>
    ValType type(const T &v)
    {
        return ValTrait<T>::type();
    }

    //  --------------------------------------------------------------------

    template <typename T>
    struct WasmValTrait
    {
        static const char *type()
        {
            throw std::runtime_error(typeid(T).name());
        }
    };

}

#endif
