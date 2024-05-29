#ifndef VAL_HPP
#define VAL_HPP

#include "context.hpp"
#include "traits.hpp"

#include <optional>
#include <variant>
#include <typeindex>

namespace cmcpp
{
    using ValType = std::type_index;
    struct None
    {
        virtual ~None() = default;
    };

    struct ValBase
    {
        std::type_index t;

        ValBase() : t(typeid(None)) {}
        ValBase(std::type_index t) : t(t) {}
        virtual ~ValBase() = default;
    };

    template <typename T>
    struct ValT : ValBase
    {
        T v;

        ValT() : ValBase(typeid(T)), v() {}
        ValT(const T &v) : ValBase(typeid(T)), v(v) {}
    };
    using Bool = ValT<bool>;
    using U8 = ValT<uint8_t>;
    using U16 = ValT<uint16_t>;
    using U32 = ValT<uint32_t>;
    using U64 = ValT<uint64_t>;
    using S8 = ValT<int8_t>;
    using S16 = ValT<int16_t>;
    using S32 = ValT<int32_t>;
    using S64 = ValT<int64_t>;
    using F32 = ValT<float32_t>;
    using F64 = ValT<float64_t>;
    using Char = ValT<char>;

    struct String : ValBase
    {
        const char8_t *ptr = nullptr;
        size_t len = 0;
        String() : ValBase(typeid(String)) {}
        String(const char8_t *ptr, size_t len) : ValBase(typeid(String)), ptr(ptr), len(len) {}
    };

    template <typename T>
    struct ListT : ValBase
    {
        std::type_index lt = typeid(T);
        std::vector<T> vs;
        ListT() : ValBase(typeid(ListT)) {}
        ListT(const std::vector<T> &vs) : ValBase(typeid(ListT)), vs(vs) {}
    };
    using List = ListT<ValBase>;

    template <typename T>
    struct FieldT : ValBase
    {
        const std::string &label;
        ValT<T> v;

        FieldT() : ValBase(typeid(FieldT)), label("") {}
        FieldT(const std::string &label) : ValBase(typeid(FieldT)), label(label) {}
        FieldT(const std::string &label, const ValT<T> &v) : ValBase(typeid(FieldT)), label(label), v(v) {}
    };
    using Field = FieldT<ValBase>;

    template <typename T>
    struct RecordT : ValBase
    {
        std::vector<FieldT<T>> fields;

        RecordT() : ValBase(typeid(RecordT)) {}
        RecordT(const std::vector<FieldT<T>> &fields) : ValBase(typeid(RecordT)), fields(fields) {}
    };
    using Record = RecordT<ValBase>;

    template <typename T>
    struct TupleT : ValBase
    {
        std::vector<T> vs;
        TupleT() : ValBase(typeid(TupleT)) {}
        TupleT(const std::vector<T> &vs) : ValBase(typeid(TupleT)), vs(vs) {}
    };
    using Tuple = TupleT<ValBase>;

    template <typename T>
    struct CaseT : ValBase
    {
        const std::string &label;
        std::optional<ValT<T>> v;
        std::optional<std::string> refines;

        CaseT() : ValBase(typeid(CaseT)), label("") {}
        CaseT(const std::string &label, const std::optional<ValT<T>> &v, const std::optional<std::string> &refines) : ValBase(typeid(CaseT)), label(label), v(v), refines(refines) {}
    };
    using Case = CaseT<ValBase>;

    struct Variant : ValBase
    {
        std::vector<Case> cases;

        Variant() : ValBase(typeid(Variant)) {}
        Variant(const std::vector<Case> &cases) : ValBase(typeid(Variant)), cases(cases) {}
    };

    struct Enum : ValBase
    {
        std::vector<std::string> labels;
        Enum() : ValBase(typeid(Enum)) {}
        Enum(const std::vector<std::string> &labels) : ValBase(typeid(Enum)), labels(labels) {}
    };

    template <typename T>
    struct OptionT : ValBase
    {
        ValT<T> v;
        OptionT() : ValBase(typeid(OptionT)) {}
        OptionT(const ValT<T> &v) : ValBase(typeid(OptionT)), v(v) {}
    };
    using Option = OptionT<ValBase>;

    template <typename T, typename U>
    struct ResultT : ValBase
    {
        std::optional<ValT<T>> ok;
        std::optional<ValT<U>> error;
        ResultT() : ValBase(typeid(ResultT)) {}
        ResultT(const ValT<T> &ok, const ValT<U> &error) : ValBase(typeid(ResultT)), ok(ok), error(error) {}
    };
    using Result = ResultT<ValBase, ValBase>;

    struct Flags : ValBase
    {
        std::vector<std::string> labels;

        Flags() : ValBase(typeid(Flags)) {}
        Flags(const std::vector<std::string> &labels) : ValBase(typeid(Flags)), labels(labels) {}
    };

    using Val = ValBase;

    ValType type(const Val &v);
    //  ----------------------------------------------------------------

    class Type
    {
    public:
        virtual ~Type() = default;
    };

    class ExternType : public Type
    {
    public:
        virtual ~ExternType() = default;
    };

    class CoreExternType : public Type
    {
    public:
        virtual ~CoreExternType() = default;
    };

    class CoreFuncType : public CoreExternType
    {
    public:
        const std::vector<std::string> &params;
        const std::vector<std::string> &results;

        CoreFuncType(const std::vector<std::string> &params, const std::vector<std::string> &results);
        virtual ~CoreFuncType() = default;
    };

    using WasmVal = std::variant<int32_t, int64_t, float32_t, float64_t>;
}

#endif
