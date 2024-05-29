#ifndef VAL2_HPP
#define VAL2_HPP

#include "traits.hpp"

#include <functional>
#include <cassert>
#include <optional>
#include <variant>
#include <any>
#include <iostream>
#include <typeindex>

namespace cmcpp2
{
    using float32_t = float;
    using float64_t = double;

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

        FieldT() : ValBase(typeid(FieldT)), label(""), v() {}
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
        std::optional<std::string> refines = std::nullopt;

        CaseT() : ValBase(typeid(CaseT)), label(""), v() {}
        CaseT(const std::string &label, const ValT<T> &v) : ValBase(typeid(CaseT)), label(label), v(v) {}
    };
    using Case = CaseT<ValBase>;

    struct Variant : ValBase
    {
        std::vector<CaseT<std::any>> cases;
        Variant() : ValBase(typeid(Variant)) {}
        Variant(const std::vector<CaseT<std::any>> &cases) : ValBase(typeid(Variant)), cases(cases) {}
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
    public:
        std::vector<std::string> labels;

        Flags() : ValBase(typeid(Flags)) {}
        Flags(const std::vector<std::string> &labels) : ValBase(typeid(Flags)), labels(labels) {}
    };

    using Val = std::variant<std::reference_wrapper<ValT<None>>,
                             std::reference_wrapper<Bool>,
                             std::reference_wrapper<U8>,
                             std::reference_wrapper<U16>,
                             std::reference_wrapper<U32>,
                             std::reference_wrapper<U64>,
                             std::reference_wrapper<F32>,
                             std::reference_wrapper<F64>,
                             std::reference_wrapper<Char>,
                             std::reference_wrapper<String>,
                             std::reference_wrapper<List>,
                             std::reference_wrapper<Field>,
                             std::reference_wrapper<Record>,
                             std::reference_wrapper<Tuple>,
                             std::reference_wrapper<Case>,
                             std::reference_wrapper<Variant>,
                             std::reference_wrapper<Enum>,
                             std::reference_wrapper<Option>,
                             std::reference_wrapper<Result>,
                             std::reference_wrapper<Flags>>;
}

#endif
