#ifndef VAL_HPP
#define VAL_HPP

#include "context.hpp"
#include "traits.hpp"

#include <optional>
#include <variant>
#include <any>

namespace cmcpp
{
    struct String;
    using StringPtr = std::shared_ptr<String>;
    struct List;
    using ListPtr = std::shared_ptr<List>;
    struct Field;
    using FieldPtr = std::shared_ptr<Field>;
    struct Record;
    using RecordPtr = std::shared_ptr<Record>;
    struct Tuple;
    using TuplePtr = std::shared_ptr<Tuple>;
    struct Case;
    using CasePtr = std::shared_ptr<Case>;
    struct Variant;
    using VariantPtr = std::shared_ptr<Variant>;
    struct Enum;
    using EnumPtr = std::shared_ptr<Enum>;
    struct Option;
    using OptionPtr = std::shared_ptr<Option>;
    struct Result;
    using ResultPtr = std::shared_ptr<Result>;
    struct Flags;
    using FlagsPtr = std::shared_ptr<Flags>;
    using Val = std::variant<void *, bool, int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float32_t, float64_t, char,
                             StringPtr,
                             ListPtr,
                             FieldPtr,
                             RecordPtr,
                             TuplePtr,
                             CasePtr,
                             VariantPtr,
                             EnumPtr,
                             OptionPtr,
                             ResultPtr,
                             FlagsPtr,
                             std::string>;

    ValType type(const Val &v);
    size_t index(ValType t);

    struct ValBase
    {
        ValType t;

        ValBase();
        ValBase(const ValType &t);
    };

    struct String : ValBase
    {
        std::string str;
        const char8_t *ptr;
        size_t len;

        String();
        String(const char8_t *ptr, size_t len);
        String(const std::string &str);

        std::string to_string() const;
    };

    struct List : ValBase
    {
        ValType lt;
        std::vector<Val> vs;

        List();
        List(ValType lt);
    };

    struct Field : ValBase
    {
        const std::string &label;
        ValType ft;
        Val v;

        Field(const std::string &label, ValType ft);
    };

    struct Record : ValBase
    {
        std::vector<Field> fields;

        Record();
    };
    struct Tuple : ValBase
    {
        std::vector<Val> vs;

        Tuple();
    };

    struct Case : ValBase
    {
        std::string label;
        std::optional<Val> v = std::nullopt;
        std::optional<std::string> refines = std::nullopt;

        Case();
        Case(const std::string &label, const std::optional<Val> &v = std::nullopt, const std::optional<std::string> &refines = std::nullopt);
    };

    struct Variant : ValBase
    {
        std::vector<Case> cases;

        Variant();
        Variant(const std::vector<Case> &cases);
    };

    struct Enum : ValBase
    {
        std::vector<std::string> labels;

        Enum();
    };

    struct Option : ValBase
    {
        Val v;

        Option();
    };

    struct Result : ValBase
    {
        std::optional<Val> ok;
        std::optional<Val> error;

        Result();
    };

    struct Flags : ValBase
    {
        std::vector<std::string> labels;

        Flags();
    };

    class WasmVal
    {
    public:
        WasmValType kind;
        WasmValVariant v;

        template <typename T>
        WasmVal(T v) : kind(WasmValTrait<T>::name()), v(v) {}
        virtual ~WasmVal() = default;

        template <typename T>
        operator T() const { return std::get<T>(v); }
    };

    // template <typename T>
    // class WasmValT : public WasmValBase
    // {
    // public:
    //     WasmValT(T v) : WasmValBase(WasmValTrait<T>::name())
    //     {
    //         this->v = v;
    //     }
    // };

    // class FuncType
    // {
    // public:
    //     virtual ~FuncType() = default;

    //     virtual std::vector<ValType> param_types() = 0;
    //     virtual std::vector<ValType> result_types() = 0;
    // };
    // FuncTypePtr createFuncType();

    // class List
    // {
    // public:
    //     const ValType &t;
    //     std::vector<Val> vs;

    //     List(const ValType &t);
    //     virtual ~List() = default;
    // };

    // class Field
    // {
    // public:
    //     const std::string &label;
    //     const ValType &t;
    //     std::optional<Val> v;

    //     Field(const std::string &label, const ValType &t);
    //     virtual ~Field() = default;
    // };

    // class Own
    // {
    // public:
    //     const ResourceType &rt;

    //     Own(const ResourceType &rt);
    //     virtual ~Own() = default;
    // };

    // class Borrow
    // {
    // public:
    //     const ResourceType &rt;

    //     Borrow(const ResourceType &rt);
    //     virtual ~Borrow() = default;
    // };
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
}

#endif
