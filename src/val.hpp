#ifndef VAL_HPP
#define VAL_HPP

#include "traits.hpp"

namespace cmcpp
{
    class string_t;
    using string_ptr = std::shared_ptr<string_t>;

    class list_t;
    using list_ptr = std::shared_ptr<list_t>;

    //  Vals  ----------------------------------------------------------------
    using Val = std::variant<bool, int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float32_t, float64_t, wchar_t, string_ptr, list_ptr>;
    bool operator==(const Val &lhs, const Val &rhs);

    class string_t
    {
    private:
        std::string str;

    public:
        const char8_t *ptr;
        size_t len;

        string_t();
        string_t(const char8_t *ptr, size_t len);
        string_t(const std::string &_str);
        ~string_t() = default;
        bool operator==(const string_t &rhs) const;

        std::string to_string() const;
    };
    template <>
    struct ValTrait<string_ptr>
    {
        static ValType type() { return ValType::String; }
    };

    class list_t
    {
    public:
        ValType lt;
        std::vector<Val> vs;

        list_t();
        list_t(const std::vector<Val> vs);
        ~list_t() = default;
        bool operator==(const list_t &rhs) const;
    };
    template <>
    struct ValTrait<list_ptr>
    {
        static ValType type() { return ValType::List; }
    };

    ValType valType(const Val &v);
    const char *valTypeName(ValType type);

    //  Refs  ----------------------------------------------------------------
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
    using Char = std::optional<wchar_t>;
    using String = std::optional<string_ptr>;
    using List = std::optional<list_ptr>;

    using Ref = std::variant<Bool, S8, U8, S16, U16, S32, U32, S64, U64, F32, F64, Char, String, List>;
    ValType refType(const Ref &v);

    // WasmVals  ------------------------------------------------------------

    using WasmVal = std::variant<int32_t, int64_t, float32_t, float64_t>;
    ValType wasmValType(const WasmVal &v);

    using WasmRef = std::variant<S32, S64, F32, F64>;
    ValType wasmRefType(const WasmRef &v);

    const char *wasmValTypeName(ValType type);
}

#endif
