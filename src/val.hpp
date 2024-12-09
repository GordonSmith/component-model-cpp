#ifndef VAL_HPP
#define VAL_HPP

#include "traits.hpp"

namespace cmcpp
{

    //  Vals  ----------------------------------------------------------------

    using Val = std::variant<bool, int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float32_t, float64_t, wchar_t, string_ptr, list_ptr, field_ptr, record_ptr, tuple_ptr, case_ptr, variant_ptr, enum_ptr, option_ptr, result_ptr, flags_ptr>;

    ValType valType(const Val &v);
    const char *valTypeName(ValType type);
    bool operator==(const Val &lhs, const Val &rhs);
    struct PrintValVisitor
    {
        std::ostream &stream;
        PrintValVisitor(std::ostream &stream);
        void operator()(const bool &v) const;
        void operator()(const int8_t &v) const;
        void operator()(const uint8_t &v) const;
        void operator()(const int16_t &v) const;
        void operator()(const uint16_t &v) const;
        void operator()(const int32_t &v) const;
        void operator()(const uint32_t &v) const;
        void operator()(const int64_t &v) const;
        void operator()(const uint64_t &v) const;
        void operator()(const float32_t &v) const;
        void operator()(const float64_t &v) const;
        void operator()(const wchar_t &v) const;
        void operator()(const string_ptr &v) const;
        void operator()(const list_ptr &v) const;
        void operator()(const field_ptr &v) const;
        void operator()(const record_ptr &v) const;
        void operator()(const tuple_ptr &v) const;
        void operator()(const case_ptr &v) const;
        void operator()(const variant_ptr &v) const;
        void operator()(const enum_ptr &v) const;
        void operator()(const option_ptr &v) const;
        void operator()(const result_ptr &v) const;
        void operator()(const flags_ptr &v) const;
    };

    //  ----------------------------------------------------------------------

    class string_t
    {
    private:
        Encoding _encoding = Encoding::Utf8;
        std::string owned_string;
        std::string_view view = std::string_view(owned_string);

        string_t();

    public:
        // Param order to match canonical ABI, caller owns string  ---
        string_t(const char *ptr, Encoding encoding, size_t len);

        // Preallocate string storage, string_t instance owns string  ---
        string_t(Encoding encoding, size_t len);

        // Caller owns string  ---
        string_t(Encoding encoding, const std::string &_str);

        ~string_t() = default;

        bool operator==(const string_t &rhs) const;

        Encoding encoding() const;
        char8_t *ptr() const;
        const char *c_str() const;
        const size_t byte_len() const;
        void resize(size_t len);
        const std::string_view &to_string() const;
    };

    class list_t
    {
    public:
        Val lt;
        std::vector<Val> vs;

        list_t();
        list_t(const Val &lt);
        list_t(const Val &lt, const std::vector<Val> &vs);
        ~list_t() = default;
        bool operator==(const list_t &rhs) const;
    };

    class field_t
    {
    public:
        std::string label;
        Val v;

        field_t();
        field_t(const std::string &label, const Val &v);
        ~field_t() = default;
        bool operator==(const field_t &rhs) const;
    };

    class record_t
    {
    public:
        std::vector<field_ptr> fields;

        record_t();
        record_t(const std::vector<field_ptr> &fields);
        ~record_t() = default;
        bool operator==(const record_t &rhs) const;

        Val find(const std::string &label) const;
    };

    class tuple_t
    {
    public:
        std::vector<Val> vs;

        tuple_t();
        tuple_t(const std::vector<Val> &vs);
        ~tuple_t() = default;
        bool operator==(const tuple_t &rhs) const;
    };

    class case_t
    {
    public:
        std::string label;
        std::optional<Val> v;
        std::optional<std::string> refines = std::nullopt;

        case_t();
        case_t(const std::string &label, const std::optional<Val> &v = std::nullopt, const std::optional<std::string> &refines = std::nullopt);
        ~case_t() = default;
        bool operator==(const case_t &rhs) const;
    };

    class variant_t
    {
    public:
        std::vector<case_ptr> cases;

        variant_t();
        variant_t(const std::vector<case_ptr> &cases);
        ~variant_t() = default;
        bool operator==(const variant_t &rhs) const;
    };

    class enum_t
    {
    public:
        std::vector<std::string> labels;

        enum_t();
        enum_t(const std::vector<std::string> &labels);
        ~enum_t() = default;
        bool operator==(const enum_t &rhs) const;
    };

    class option_t
    {
    public:
        Val v;

        option_t();
        option_t(const Val &v);
        ~option_t() = default;
        bool operator==(const option_t &rhs) const;
    };

    class result_t
    {
    public:
        Val ok;
        Val error;

        result_t();
        result_t(const Val &ok, const Val &error);
        ~result_t() = default;
        bool operator==(const result_t &rhs) const;
    };

    class flags_t
    {
    public:
        std::vector<std::string> labels;

        flags_t();
        flags_t(const std::vector<std::string> &labels);
        ~flags_t() = default;
        bool operator==(const flags_t &rhs) const;
    };

    // WasmVals  ------------------------------------------------------------

    using WasmVal = std::variant<int32_t, int64_t, float32_t, float64_t>;
    ValType wasmValType(const WasmVal &v);

    const char *wasmValTypeName(ValType type);
}

#endif
