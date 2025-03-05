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
#include <cmath>

#include "boost/pfr.hpp"

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
    using WasmValTypeVector = std::vector<WasmValType>;
    using WasmVal = std::variant<int32_t, int64_t, float32_t, float64_t>;
    using WasmValVector = std::vector<WasmVal>;

    template <typename T>
    struct WasmValTrait
    {
        static constexpr WasmValType type = WasmValType::UNKNOWN;
        static_assert(WasmValTrait<T>::type != WasmValType::UNKNOWN, "T must be valid WasmValType.");
    };

    template <>
    struct WasmValTrait<int32_t>
    {
        static constexpr WasmValType type = WasmValType::i32;
        static constexpr uint32_t size = 4;
    };

    template <>
    struct WasmValTrait<int64_t>
    {
        static constexpr WasmValType type = WasmValType::i64;
        static constexpr uint32_t size = 8;
    };

    template <>
    struct WasmValTrait<float32_t>
    {
        static constexpr WasmValType type = WasmValType::f32;
        static constexpr uint32_t size = 4;
    };

    template <>
    struct WasmValTrait<float64_t>
    {
        static constexpr WasmValType type = WasmValType::f64;
        static constexpr uint32_t size = 8;
    };

    template <typename T>
    concept FlatValue =
        WasmValTrait<T>::type == WasmValType::i32 ||
        WasmValTrait<T>::type == WasmValType::i64 ||
        WasmValTrait<T>::type == WasmValType::f32 ||
        WasmValTrait<T>::type == WasmValType::f64;

    template <WasmValType T>
    struct WasmValTypeTrait;

    template <>
    struct WasmValTypeTrait<WasmValType::i32>
    {
        using type = int32_t;
    };

    template <>
    struct WasmValTypeTrait<WasmValType::i64>
    {
        using type = int64_t;
    };

    template <>
    struct WasmValTypeTrait<WasmValType::f32>
    {
        using type = float32_t;
    };

    template <>
    struct WasmValTypeTrait<WasmValType::f64>
    {
        using type = float64_t;
    };

    //  --------------------------------------------------------------------

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
        static_assert(false, "T is not a valid type for ValTrait. Type: ");
        static constexpr ValType type = ValType::UNKNOWN;
        using inner_type = std::monostate;
        static constexpr uint32_t size = 0;
        static constexpr uint32_t alignment = 0;
        static constexpr std::array<WasmValType, 0> flat_types = {};
    };

    //  Boolean  --------------------------------------------------------------------
    using bool_t = bool;

    template <>
    struct ValTrait<bool_t>
    {
        static constexpr ValType type = ValType::Bool;
        using inner_type = bool;
        static constexpr uint32_t size = 1;
        static constexpr uint32_t alignment = 1;
        static constexpr std::array<WasmValType, 1> flat_types = {WasmValType::i32};
    };
    template <typename T>
    concept Boolean = ValTrait<T>::type == ValType::Bool;
    //  Char  --------------------------------------------------------------------
    using char_t = char32_t;

    template <>
    struct ValTrait<char_t>
    {
        static constexpr ValType type = ValType::Char;
        using inner_type = char32_t;
        static constexpr uint32_t size = 4;
        static constexpr uint32_t alignment = 4;
        static constexpr std::array<WasmValType, 1> flat_types = {WasmValType::i32};
    };

    template <typename T>
    concept Char = ValTrait<T>::type == ValType::Char;

    //  Numerics  --------------------------------------------------------------------
    template <>
    struct ValTrait<int8_t>
    {
        static constexpr ValType type = ValType::S8;
        using inner_type = int8_t;
        static constexpr uint32_t size = 1;
        static constexpr uint32_t alignment = 1;
        static constexpr std::array<WasmValType, 1> flat_types = {WasmValType::i32};

        static constexpr int8_t LOW_VALUE = std::numeric_limits<int8_t>::lowest();
        static constexpr int8_t HIGH_VALUE = std::numeric_limits<int8_t>::max();
    };

    template <>
    struct ValTrait<uint8_t>
    {
        static constexpr ValType type = ValType::U8;
        using inner_type = uint8_t;
        static constexpr uint32_t size = 1;
        static constexpr uint32_t alignment = 1;
        static constexpr std::array<WasmValType, 1> flat_types = {WasmValType::i32};

        static constexpr int8_t LOW_VALUE = std::numeric_limits<uint8_t>::lowest();
        static constexpr int8_t HIGH_VALUE = std::numeric_limits<uint8_t>::max();
    };

    template <>
    struct ValTrait<int16_t>
    {
        static constexpr ValType type = ValType::S16;
        using inner_type = int16_t;
        static constexpr uint32_t size = 2;
        static constexpr uint32_t alignment = 2;
        static constexpr std::array<WasmValType, 1> flat_types = {WasmValType::i32};

        static constexpr int16_t LOW_VALUE = std::numeric_limits<int16_t>::lowest();
        static constexpr int16_t HIGH_VALUE = std::numeric_limits<int16_t>::max();
    };

    template <>
    struct ValTrait<uint16_t>
    {
        static constexpr ValType type = ValType::U16;
        using inner_type = uint16_t;
        static constexpr uint32_t size = 2;
        static constexpr uint32_t alignment = 2;
        static constexpr std::array<WasmValType, 1> flat_types = {WasmValType::i32};

        static constexpr uint16_t LOW_VALUE = std::numeric_limits<uint16_t>::lowest();
        static constexpr uint16_t HIGH_VALUE = std::numeric_limits<uint16_t>::max();
    };

    template <>
    struct ValTrait<int32_t>
    {
        static constexpr ValType type = ValType::S32;
        using inner_type = int32_t;
        static constexpr uint32_t size = 4;
        static constexpr uint32_t alignment = 4;
        static constexpr std::array<WasmValType, 1> flat_types = {WasmValType::i32};

        static constexpr int32_t LOW_VALUE = std::numeric_limits<int32_t>::lowest();
        static constexpr int32_t HIGH_VALUE = std::numeric_limits<int32_t>::max();
    };

    template <>
    struct ValTrait<uint32_t>
    {
        static constexpr ValType type = ValType::U32;
        using inner_type = uint32_t;
        static constexpr uint32_t size = 4;
        static constexpr uint32_t alignment = 4;
        static constexpr std::array<WasmValType, 1> flat_types = {WasmValType::i32};

        static constexpr uint32_t LOW_VALUE = std::numeric_limits<uint32_t>::lowest();
        static constexpr uint32_t HIGH_VALUE = std::numeric_limits<uint32_t>::max();
    };

    template <>
    struct ValTrait<int64_t>
    {
        static constexpr ValType type = ValType::S64;
        using inner_type = int64_t;
        static constexpr uint32_t size = 8;
        static constexpr uint32_t alignment = 8;
        static constexpr std::array<WasmValType, 1> flat_types = {WasmValType::i64};

        static constexpr int64_t LOW_VALUE = std::numeric_limits<int64_t>::lowest();
        static constexpr int64_t HIGH_VALUE = std::numeric_limits<int64_t>::max();
    };

    template <>
    struct ValTrait<uint64_t>
    {
        static constexpr ValType type = ValType::U64;
        using inner_type = uint64_t;
        static constexpr uint32_t size = 8;
        static constexpr uint32_t alignment = 8;
        static constexpr std::array<WasmValType, 1> flat_types = {WasmValType::i64};

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
        using inner_type = float32_t;
        static constexpr uint32_t size = 4;
        static constexpr uint32_t alignment = 4;
        static constexpr std::array<WasmValType, 1> flat_types = {WasmValType::f32};

        static constexpr float32_t LOW_VALUE = std::numeric_limits<float32_t>::lowest();
        static constexpr float32_t HIGH_VALUE = std::numeric_limits<float32_t>::max();
    };

    template <>
    struct ValTrait<float64_t>
    {
        static constexpr ValType type = ValType::F64;
        using inner_type = float64_t;
        static constexpr uint32_t size = 8;
        static constexpr uint32_t alignment = 8;
        static constexpr std::array<WasmValType, 1> flat_types = {WasmValType::f64};

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
    enum class Encoding
    {
        Latin1,
        Utf8,
        Utf16,
        Latin1_Utf16
    };

    const uint32_t UTF16_TAG = 1U << 31;

    using string_t = std::string;
    template <>
    struct ValTrait<string_t>
    {
        static constexpr ValType type = ValType::String;
        using inner_type = char;
        static constexpr uint32_t size = 8;
        static constexpr uint32_t alignment = 4;
        static constexpr std::array<WasmValType, 2> flat_types = {WasmValType::i32, WasmValType::i32};

        static constexpr Encoding encoding = Encoding::Utf8;
        static constexpr size_t char_size = sizeof(char);
    };

    using u16string_t = std::u16string;
    template <>
    struct ValTrait<u16string_t>
    {
        static constexpr ValType type = ValType::String;
        using inner_type = char16_t;
        static constexpr uint32_t size = 8;
        static constexpr uint32_t alignment = 4;
        static constexpr std::array<WasmValType, 2> flat_types = {WasmValType::i32, WasmValType::i32};

        static constexpr Encoding encoding = Encoding::Utf16;
        static constexpr size_t char_size = sizeof(char16_t);
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
        static constexpr ValType type = ValType::String;
        using inner_type = char8_t;
        static constexpr uint32_t size = 8;
        static constexpr uint32_t alignment = 4;
        static constexpr std::array<WasmValType, 2> flat_types = {WasmValType::i32, WasmValType::i32};

        static constexpr Encoding encoding = Encoding::Latin1_Utf16;
        static constexpr size_t char_size = sizeof(char8_t);
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
        static constexpr std::array<WasmValType, 2> flat_types = {WasmValType::i32, WasmValType::i32};
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
        using inner_type = std::bitset<sizeof...(Ts)>;
        static constexpr auto size = byteSize<Ts...>();
        static constexpr auto alignment = byteSize<Ts...>();
        static constexpr std::array<WasmValType, 1> flat_types = {WasmValType::i32};
    };

    template <typename T>
    concept Flags = ValTrait<T>::type == ValType::Flags;

    //  Field  --------------------------------------------------------------------
    template <typename T>
    concept Field = ValTrait<T>::type != ValType::UNKNOWN;

    //  Tuple  --------------------------------------------------------------------
    inline constexpr uint32_t align_to(uint32_t ptr, uint8_t alignment)
    {
        return (ptr + static_cast<int32_t>(alignment) - 1) & ~(static_cast<int32_t>(alignment) - 1);
    }

    template <Field... Ts>
    using tuple_t = std::tuple<Ts...>;
    template <Field... Ts>
    struct ValTrait<tuple_t<Ts...>>
    {
        static constexpr ValType type = ValType::Tuple;
        using inner_type = typename std::tuple<Ts...>;
        static constexpr uint32_t alignment = []() constexpr
        {
            uint32_t a = 1;
            ((a = std::max(a, ValTrait<Ts>::alignment)), ...);
            return a;
        }();
        static constexpr uint32_t size = []() constexpr
        {
            uint32_t s = 0;
            ((s = align_to(s, ValTrait<Ts>::alignment), (s += ValTrait<Ts>::size)), ...);
            return align_to(s, alignment);
        }();
        static constexpr size_t flat_types_len = []() constexpr
        {
            size_t i = 0;
            ((i += ValTrait<Ts>::flat_types.size()), ...);
            return i;
        }();
        static constexpr std::array<WasmValType, flat_types_len> flat_types = []() constexpr
        {
            std::array<WasmValType, flat_types_len> v;
            size_t i = 0;
            (([&v, &i]()
              {
                for (auto &ft : ValTrait<Ts>::flat_types) {
                    v[i++] = ft;
                } }()),
             ...);
            return v;
        }();
    };
    template <typename T>
    concept Tuple = ValTrait<T>::type == ValType::Tuple;

    //  Record  ------------------------------------------------------------------
    template <typename S>
    concept Struct = (std::is_aggregate_v<S>);

    template <Struct R>
    struct record_t : R
    {
    };

    template <Struct R>
    struct ValTrait<record_t<R>>
    {
        static constexpr ValType type = ValType::Record;
        using inner_type = R;
        using tuple_type = decltype(boost::pfr::structure_to_tuple(std::declval<R>()));
        static constexpr size_t flat_types_len = ValTrait<tuple_type>::flat_types_len;
        static constexpr std::array<WasmValType, flat_types_len> flat_types = ValTrait<tuple_type>::flat_types;
    };
    template <typename T>
    concept Record = ValTrait<T>::type == ValType::Record;

    template <Record R, Tuple T, std::size_t... I>
    R to_struct_impl(const T &t, std::index_sequence<I...>)
    {
        return R{std::get<I>(t)...};
    }

    template <Record R, Tuple T>
    R to_struct(const T &t)
    {
        return to_struct_impl<R>(t, std::make_index_sequence<std::tuple_size_v<T>>{});
    }

    //  Variant  ------------------------------------------------------------------
    inline constexpr WasmValType join(WasmValType a, WasmValType b)
    {
        if (a == b)
        {
            return a;
        }
        if ((a == WasmValType::i32 && b == WasmValType::f32) || (a == WasmValType::f32 && b == WasmValType::i32))
        {
            return WasmValType::i32;
        }
        return WasmValType::i64;
    }

    template <Field... Ts>
    using variant_t = std::variant<Ts...>;
    template <Field... Ts>
    struct ValTrait<variant_t<Ts...>>
    {
        static constexpr ValType type = ValType::Variant;
        using inner_type = typename std::variant<Ts...>;

        static constexpr int match = static_cast<int>(std::ceil(std::log2(std::variant_size_v<inner_type>) / 8.0));
        using discriminant_type = std::conditional_t<match == 0, uint8_t, std::conditional_t<match == 1, uint8_t, std::conditional_t<match == 2, uint16_t, std::conditional_t<match == 3, uint32_t, void>>>>;
        static constexpr uint32_t max_case_alignment = []() constexpr
        {
            uint32_t a = 1;
            ((a = std::max(a, ValTrait<Ts>::alignment)), ...);
            return a;
        }();
        static constexpr uint32_t alignment_variant = std::max(ValTrait<discriminant_type>::alignment, max_case_alignment);
        static constexpr uint32_t alignment = std::max(ValTrait<discriminant_type>::alignment, static_cast<uint32_t>(1));
        static constexpr uint32_t max_case_size = []() constexpr
        {
            uint32_t cs = 0;
            ((cs = std::max(cs, ValTrait<Ts>::size)), ...);
            return cs;
        }();
        static constexpr uint32_t size = []() constexpr
        {
            uint32_t s = ValTrait<discriminant_type>::size;
            s = align_to(s, max_case_alignment);
            s += max_case_size;
            return align_to(s, alignment_variant);
        }();
        static_assert(size > 0 && size < std::numeric_limits<uint32_t>::max());
        static constexpr size_t flat_types_len = []() constexpr
        {
            size_t i = 0;
            ((i = std::max(i, ValTrait<Ts>::flat_types.size())), ...);
            return i + 1;
        }();
        static constexpr std::array<WasmValType, flat_types_len> flat_types = []() constexpr
        {
            std::array<WasmValType, flat_types_len> flat;
            flat.fill(WasmValType::i32);
            flat[0] = ValTrait<discriminant_type>::flat_types[0];
            ([&]()
             {
                size_t i = 1;
                for (auto &ft : ValTrait<Ts>::flat_types) {
                    flat[i] = join(flat[i], ft);
                    ++i;
                } }(), ...);
            return flat;
        }();
    };
    template <typename T>
    concept Variant = ValTrait<T>::type == ValType::Variant;

    //  Option  --------------------------------------------------------------------
    template <Field T>
    using option_t = std::optional<T>;
    template <Field T>
    struct ValTrait<option_t<T>>
    {
        static constexpr ValType type = ValType::Option;
        using inner_type = T;
        static constexpr uint32_t size = ValTrait<variant_t<bool_t, T>>::size;
        static constexpr uint32_t alignment = ValTrait<variant_t<bool_t, T>>::size;
        static constexpr auto flat_types = ValTrait<variant_t<bool_t, T>>::flat_types;
    };
    template <typename T>
    concept Option = ValTrait<T>::type == ValType::Option;

    //  --------------------------------------------------------------------

    using offset = uint32_t;
    using bytes = uint32_t;
}

#endif
