#ifndef CMCPP_TRAITS_HPP
#define CMCPP_TRAITS_HPP

#include <cstdint>
#include <vector>
#include <map>
#include <string_view>
#include <bitset>
#include <optional>
#include <variant>
#include <memory>
#include <stdexcept>
#include <array>
#include <cassert>
#include <limits>
#include <cstring>

//  See canonical ABI:
//  https://github.com/WebAssembly/component-model/blob/main/design/mvp/canonical-abi/definitions.py
//  https://github.com/WebAssembly/component-model/blob/main/design/mvp/CanonicalABI.md

namespace cmcpp
{
    using float32_t = float;
    using float64_t = double;

    enum class WasmValType : uint8_t
    {
        UNKNOWN,
        i32,
        i64,
        f32,
        f64,
        LAST
    };
    using WasmVal = std::variant<int32_t, int64_t, float32_t, float64_t>;
    using WasmValVector = std::vector<WasmVal>;
    class WasmValVectorIterator
    {
        mutable WasmValVector::const_iterator it;
        WasmValVector::const_iterator end;

    public:
        WasmValVectorIterator(const WasmValVector &v);

        template <typename T>
        T next() const
        {
            assert(it != end);
            return std::get<T>(*it++);
        }
    };

    template <typename T>
    struct WasmValTrait
    {
        static constexpr WasmValType type = WasmValType::UNKNOWN;
        // static_assert(WasmValTrait<T>::type != WasmValType::UNKNOWN, "T must be valid WasmValType.");
    };

    template <>
    struct WasmValTrait<int32_t>
    {
        static constexpr WasmValType type = WasmValType::i32;
    };

    template <>
    struct WasmValTrait<int64_t>
    {
        static constexpr WasmValType type = WasmValType::i64;
    };

    template <>
    struct WasmValTrait<float32_t>
    {
        static constexpr WasmValType type = WasmValType::f32;
    };

    template <>
    struct WasmValTrait<float64_t>
    {
        static constexpr WasmValType type = WasmValType::f64;
    };

    template <typename T>
    concept WasmValT =
        WasmValTrait<T>::type == WasmValType::i32 ||
        WasmValTrait<T>::type == WasmValType::i64 ||
        WasmValTrait<T>::type == WasmValType::f32 ||
        WasmValTrait<T>::type == WasmValType::f64;

    //  --------------------------------------------------------------------

    enum class Encoding
    {
        Latin1,
        Utf8,
        Utf16,
        Latin1_Utf16
    };

    enum class ValType : uint8_t
    {
        UNKNOWN,
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
        Borrow,
        LAST
    };

    //  ValTrait ---------------------------------------------------------------
    template <typename T>
    struct ValTrait
    {
        static constexpr ValType type = ValType::UNKNOWN;
        using inner_type = void;
        static constexpr uint32_t size = 0;
        static constexpr uint32_t alignment = 0;
        using flat_type = void;
        using flat_type_0 = void;
        using flat_type_1 = void;
    };

    //  Boolean  --------------------------------------------------------------------
    using bool_t = bool;

    template <>
    struct ValTrait<bool_t>
    {
        static constexpr ValType type = ValType::Bool;
        static constexpr uint32_t size = 1;
        static constexpr uint32_t alignment = 1;
        using flat_type = int32_t;
    };
    template <typename T>
    concept Boolean = ValTrait<T>::type == ValType::Bool;
    //  Char  --------------------------------------------------------------------
    using char_t = char32_t;

    template <>
    struct ValTrait<char_t>
    {
        static constexpr ValType type = ValType::Char;
        static constexpr uint32_t size = 4;
        static constexpr uint32_t alignment = 4;
        using flat_type = int32_t;
    };

    template <typename T>
    concept Char = ValTrait<T>::type == ValType::Char;
    //  Numerics  --------------------------------------------------------------------

    template <>
    struct ValTrait<int8_t>
    {
        static constexpr ValType type = ValType::S8;
        static constexpr uint32_t size = 1;
        static constexpr uint32_t alignment = 1;
        using flat_type = int32_t;

        static constexpr int8_t LOW_VALUE = std::numeric_limits<int8_t>::lowest();
        static constexpr int8_t HIGH_VALUE = std::numeric_limits<int8_t>::max();
    };

    template <>
    struct ValTrait<uint8_t>
    {
        static constexpr ValType type = ValType::U8;
        static constexpr uint32_t size = 1;
        static constexpr uint32_t alignment = 1;
        using flat_type = int32_t;

        static constexpr int8_t LOW_VALUE = std::numeric_limits<uint8_t>::lowest();
        static constexpr int8_t HIGH_VALUE = std::numeric_limits<uint8_t>::max();
    };

    template <>
    struct ValTrait<int16_t>
    {
        static constexpr ValType type = ValType::S16;
        static constexpr uint32_t size = 2;
        static constexpr uint32_t alignment = 2;
        using flat_type = int32_t;

        static constexpr int16_t LOW_VALUE = std::numeric_limits<int16_t>::lowest();
        static constexpr int16_t HIGH_VALUE = std::numeric_limits<int16_t>::max();
    };

    template <>
    struct ValTrait<uint16_t>
    {
        static constexpr ValType type = ValType::U16;
        static constexpr uint32_t size = 2;
        static constexpr uint32_t alignment = 2;
        using flat_type = int32_t;

        static constexpr uint16_t LOW_VALUE = std::numeric_limits<uint16_t>::lowest();
        static constexpr uint16_t HIGH_VALUE = std::numeric_limits<uint16_t>::max();
    };

    template <>
    struct ValTrait<int32_t>
    {
        static constexpr ValType type = ValType::S32;
        static constexpr uint32_t size = 4;
        static constexpr uint32_t alignment = 4;
        using flat_type = int32_t;

        static constexpr int32_t LOW_VALUE = std::numeric_limits<int32_t>::lowest();
        static constexpr int32_t HIGH_VALUE = std::numeric_limits<int32_t>::max();
    };

    template <>
    struct ValTrait<uint32_t>
    {
        static constexpr ValType type = ValType::U32;
        static constexpr uint32_t size = 4;
        static constexpr uint32_t alignment = 4;
        using flat_type = int32_t;

        static constexpr uint32_t LOW_VALUE = std::numeric_limits<uint32_t>::lowest();
        static constexpr uint32_t HIGH_VALUE = std::numeric_limits<uint32_t>::max();
    };

    template <>
    struct ValTrait<int64_t>
    {
        static constexpr ValType type = ValType::S64;
        static constexpr uint32_t size = 8;
        static constexpr uint32_t alignment = 8;
        using flat_type = int64_t;

        static constexpr int64_t LOW_VALUE = std::numeric_limits<int64_t>::lowest();
        static constexpr int64_t HIGH_VALUE = std::numeric_limits<int64_t>::max();
    };

    template <>
    struct ValTrait<uint64_t>
    {
        static constexpr ValType type = ValType::U64;
        static constexpr uint32_t size = 8;
        static constexpr uint32_t alignment = 8;
        using flat_type = int64_t;

        static constexpr uint64_t LOW_VALUE = std::numeric_limits<uint64_t>::lowest();
        static constexpr uint64_t HIGH_VALUE = std::numeric_limits<uint64_t>::max();
    };

    template <typename T>
    concept Integer = ValTrait<T>::type == ValType::S8 || ValTrait<T>::type == ValType::S16 || ValTrait<T>::type == ValType::S32 || ValTrait<T>::type == ValType::S64 ||
                      ValTrait<T>::type == ValType::U8 || ValTrait<T>::type == ValType::U16 || ValTrait<T>::type == ValType::U32 || ValTrait<T>::type == ValType::U64;

    template <typename T>
    concept SignedInteger = std::is_signed_v<T> && Integer<T>;

    template <typename T>
    concept UnsignedInteger = !std::is_signed_v<T> && Integer<T>;

    template <>
    struct ValTrait<float32_t>
    {
        static constexpr ValType type = ValType::F32;
        static constexpr uint32_t size = 4;
        static constexpr uint32_t alignment = 4;
        using flat_type = float32_t;

        static constexpr float32_t LOW_VALUE = std::numeric_limits<float32_t>::lowest();
        static constexpr float32_t HIGH_VALUE = std::numeric_limits<float32_t>::max();
    };

    template <>
    struct ValTrait<float64_t>
    {
        static constexpr ValType type = ValType::F64;
        static constexpr uint32_t size = 8;
        static constexpr uint32_t alignment = 8;
        using flat_type = float64_t;

        static constexpr float64_t LOW_VALUE = std::numeric_limits<float64_t>::lowest();
        static constexpr float64_t HIGH_VALUE = std::numeric_limits<float64_t>::max();
    };

    template <typename T>
    concept Float = ValTrait<T>::type == ValType::F32 || ValTrait<T>::type == ValType::F64;

    template <typename T>
    concept Numeric = Integer<T> || Float<T>;

    template <typename T>
    concept Byte = ValTrait<T>::type == ValType::U8 || ValTrait<T>::type == ValType::S8;

    template <typename T>
    concept HalfWord = ValTrait<T>::type == ValType::U16 || ValTrait<T>::type == ValType::S16;

    template <typename T>
    concept Word = ValTrait<T>::type == ValType::U32 || ValTrait<T>::type == ValType::S32 || ValTrait<T>::type == ValType::F32;

    template <typename T>
    concept DoubleWord = ValTrait<T>::type == ValType::U64 || ValTrait<T>::type == ValType::S64 || ValTrait<T>::type == ValType::F64;

    //  Strings --------------------------------------------------------------------
    const uint32_t UTF16_TAG = 1U << 31;

    using string_t = std::string;
    template <>
    struct ValTrait<string_t>
    {
        static constexpr Encoding encoding = Encoding::Utf8;
        static constexpr ValType type = ValType::String;
        static constexpr size_t char_size = sizeof(char);
        using inner_type = char;
        static constexpr uint32_t size = 8;
        static constexpr uint32_t alignment = 4;
        using flat_type_0 = int32_t;
        using flat_type_1 = int32_t;
    };

    using u16string_t = std::u16string;
    template <>
    struct ValTrait<u16string_t>
    {
        static constexpr Encoding encoding = Encoding::Utf16;
        static constexpr ValType type = ValType::String;
        static constexpr size_t char_size = sizeof(char16_t);
        using inner_type = char16_t;
        static constexpr uint32_t size = 8;
        static constexpr uint32_t alignment = 4;
        using flat_type_0 = int32_t;
        using flat_type_1 = int32_t;
    };

    //  Do we really need to support this on the host?
    struct latin1_u16string_t
    {
        Encoding encoding = Encoding::Latin1;
        string_t str;
        u16string_t u16str;

        inline void resize(size_t new_size)
        {
            encoding == Encoding::Latin1 ? str.resize(new_size) : u16str.resize(new_size);
        }
        inline void *data() const
        {
            return encoding == Encoding::Latin1 ? (void *)str.data() : (void *)u16str.data();
        }
    };
    template <>
    struct ValTrait<latin1_u16string_t>
    {
        static constexpr Encoding encoding = Encoding::Latin1_Utf16;
        static constexpr ValType type = ValType::String;
        static constexpr size_t char_size = sizeof(char8_t);
        static constexpr uint32_t size = 8;
        static constexpr uint32_t alignment = 4;
        using flat_type_0 = int32_t;
        using flat_type_1 = int32_t;
    };

    template <typename T>
    concept String = ValTrait<T>::type == ValType::String;
    //  List  --------------------------------------------------------------------
    template <typename T>
    using list_t = std::vector<T>;
    template <typename T>
    struct ValTrait<list_t<T>>
    {
        static constexpr ValType type = ValType::List;
        using inner_type = T;
        static constexpr uint32_t size = 8;
        static constexpr uint32_t alignment = 4;
        // static constexpr WasmValTypeVector() using flat_type_0 = int32_t;
        using flat_type_1 = int32_t;
    };
    template <typename T>
    concept List = ValTrait<T>::type == ValType::List;
    //  Flags  --------------------------------------------------------------------
    template <size_t N>
    struct StringLiteral
    {
        constexpr StringLiteral(const char (&str)[N])
        {
            std::copy(str, str + N, value);
        }
        char value[N];
        constexpr std::string_view view() const
        {
            return std::string_view(value, N - 1);
        }
    };

    template <StringLiteral... Ts>
    struct flags_t : std::bitset<sizeof...(Ts)>
    {
        static constexpr size_t labelsSize = sizeof...(Ts);
        static constexpr std::array<const char *, labelsSize> labels = {Ts.value...};

        template <StringLiteral T>
        bool test() const
        {
            constexpr size_t index = getIndex<T>();
            return std::bitset<sizeof...(Ts)>::test(index);
        }

        template <StringLiteral T>
        void set()
        {
            constexpr size_t index = getIndex<T>();
            std::bitset<sizeof...(Ts)>::set(index);
        }

        template <StringLiteral T>
        void reset()
        {
            constexpr size_t index = getIndex<T>();
            std::bitset<sizeof...(Ts)>::reset(index);
        }

    private:
        static constexpr int constexpr_strcmp(const char *str1, const char *str2)
        {
            while (*str1 && (*str1 == *str2))
            {
                ++str1;
                ++str2;
            }
            return (static_cast<unsigned char>(*str1) - static_cast<unsigned char>(*str2));
        }

        template <StringLiteral T, size_t Index = 0>
        static constexpr size_t getIndex()
        {
            if constexpr (Index < labelsSize)
            {
                return constexpr_strcmp(labels[Index], T.value) == 0 ? Index : getIndex<T, Index + 1>();
            }
            else
            {
                return -1;
            }
        }
    };

    template <StringLiteral... Ts>
    constexpr size_t byteSize()
    {
        size_t n = sizeof...(Ts);
        assert(0 < n && n <= 32);
        if (n <= 8)
            return 1;
        if (n <= 16)
            return 2;
        return 4;
    }

    template <StringLiteral... Ts>
    struct ValTrait<flags_t<Ts...>>
    {
        static constexpr ValType type = ValType::Flags;
        static constexpr auto size = byteSize<Ts...>();
        static constexpr auto alignment = byteSize<Ts...>();
        using flat_type = int32_t;
    };

    template <typename T>
    concept Flags = ValTrait<T>::type == ValType::Flags;
    //  Record  --------------------------------------------------------------------
    template <typename T>
    concept Field = ValTrait<T>::type != ValType::UNKNOWN;

    template <Field... Ts>
    using record_t = std::tuple<Ts...>;
    template <Field... Ts>
    struct ValTrait<record_t<Ts...>>
    {
        static constexpr ValType type = ValType::Record;
        using inner_type = typename std::tuple<Ts...>;
    };
    template <typename T>
    concept Record = ValTrait<T>::type == ValType::Record;

    template <typename T, Record R, std::size_t... I>
    T to_struct_impl(const R &t, std::index_sequence<I...>)
    {
        return T{std::get<I>(t)...};
    }

    template <typename T, Record R>
    T to_struct(const R &t)
    {
        return to_struct_impl<T>(t, std::make_index_sequence<std::tuple_size_v<R>>{});
    }

    //  Variant  ------------------------------------------------------------------

    template <Field... Ts>
    using variant_t = std::variant<Ts...>;
    template <Field... Ts>
    struct ValTrait<variant_t<Ts...>>
    {
        static constexpr ValType type = ValType::Variant;
        using inner_type = typename std::variant<Ts...>;
    };
    template <typename T>
    concept Variant = ValTrait<T>::type == ValType::Variant;

    //  Other  --------------------------------------------------------------------

    // template <typename... Ts>
    // using tuple_t = std::tuple<Ts...>;
    // template <typename... Ts>
    // struct ValTrait<tuple_t<Ts...>>
    // {
    //     static constexpr ValType type = ValType::Tuple;
    // };

    // template <typename T>
    // struct case_t
    // {
    //     std::string label;
    //     std::optional<T> v;
    // };
    // template <typename T>
    // struct ValTrait<case_t<T>>
    // {
    //     static constexpr ValType type = ValType::Case;
    //     using inner_type = T;
    // };

    // template <typename... Ts>
    // using variant_t = std::variant<Ts...>;
    // template <typename... Ts>
    // struct ValTrait<variant_t<Ts...>>
    // {
    //     static constexpr ValType type = ValType::Variant;
    // };

    // using enum_t = std::vector<std::string>;
    // template <>
    // struct ValTrait<enum_t>
    // {
    //     static constexpr ValType type = ValType::Enum;
    // };

    // class option_t;
    // using option_ptr = std::shared_ptr<option_t>;
    // template <>
    // struct ValTrait<option_ptr>
    // {
    //     static ValType type() { return ValType::Option; }
    // };

    // class result_t;
    // using result_ptr = std::shared_ptr<result_t>;
    // template <>
    // struct ValTrait<result_ptr>
    // {
    //     static ValType type() { return ValType::Result; }
    // };

    // class flags_t;
    // using flags_ptr = std::shared_ptr<flags_t>;
    // template <>
    // struct ValTrait<flags_ptr>
    // {
    //     static ValType type() { return ValType::Flags; }
    // };

    //  --------------------------------------------------------------------

    using offset = uint32_t;
    using bytes = uint32_t;
}

#endif
