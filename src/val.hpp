#include "cmcpp.hpp"

#if __has_include(<span>)
#include <span>
#else
#include <string>
#include <sstream>
#endif

#include <optional>
#include <string>
#include <variant>

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

typedef struct string_
{
    const char *ptr;
    size_t len;
} string_t;

typedef struct func
{
    uint64_t store_id;
    size_t index;
} func_t;

// class FuncType;
// using FuncTypePtr = std::shared_ptr<FuncType>;

class List;
using ListPtr = std::shared_ptr<List>;
class Field;
using FieldPtr = std::shared_ptr<Field>;
class Record;
using RecordPtr = std::shared_ptr<Record>;
class Tuple;
using TuplePtr = std::shared_ptr<Tuple>;
class Case;
using CasePtr = std::shared_ptr<Case>;
class Variant;
using VariantPtr = std::shared_ptr<Variant>;
class Enum;
using EnumPtr = std::shared_ptr<Enum>;
class Option;
using OptionPtr = std::shared_ptr<Option>;
class Result;
using ResultPtr = std::shared_ptr<Result>;
// class Flags;
// using FlagsPtr = std::shared_ptr<Flags>;
// class Own;
// using OwnPtr = std::shared_ptr<Own>;
// class Borrow;
// using BorrowPtr = std::shared_ptr<Borrow>;

using PtrVariant = std::variant<ListPtr, FieldPtr, RecordPtr, TuplePtr, CasePtr, VariantPtr, EnumPtr, OptionPtr, ResultPtr>;

typedef union valunion
{
    bool b;
    int8_t s8;
    uint8_t u8;
    int16_t s16;
    uint16_t u16;
    int32_t s32;
    uint32_t u32;
    int64_t s64;
    uint64_t u64;
    float32_t f32;
    float64_t f64;
    char c;
    string_t s;
    List *list;
    Field *field;
    Record *record;
    Tuple *tuple;
    Case *case_;
    Variant *variant;
    Enum *enum_;
    Option *option;
    Result *result;
    // Flags *flags;
    // Own *own;
    // Borrow *borrow;

    // FuncType *func;
} valunion_t;

typedef struct val
{
    ValType kind;
    valunion_t of;
} val_t;

class Val
{
    val_t val;
    PtrVariant shared_ptr;

    Val();
    Val(val_t val);

public:
    Val(bool b);
    Val(int8_t s8);
    Val(uint8_t u8);
    Val(int16_t s16);
    Val(uint16_t u16);
    Val(int32_t s32);
    Val(uint32_t u32);
    Val(int64_t s64);
    Val(uint64_t u64);
    Val(float32_t f32);
    Val(float64_t f64);
    Val(char c);
    Val(const char *s);
    Val(const char *s, size_t len);
    Val(ListPtr list);
    Val(FieldPtr field);
    Val(RecordPtr record);
    Val(TuplePtr tuple);
    Val(CasePtr case_);
    Val(VariantPtr variant);
    Val(EnumPtr enum_);
    Val(OptionPtr option);
    Val(ResultPtr result);
    // Val(FlagsPtr flags);
    // Val(OwnPtr own);
    // Val(BorrowPtr borrow);
    // Val(FuncTypePtr func);

    Val(const Val &other);
    Val(Val &&other) noexcept;
    ~Val();
    Val &operator=(const Val &other) noexcept;
    Val &operator=(Val &&other) noexcept;

    ValType kind() const;
    bool b() const;
    int8_t s8() const;
    uint8_t u8() const;
    int16_t s16() const;
    uint16_t u16() const;
    int32_t s32() const;
    uint32_t u32() const;
    int64_t s64() const;
    uint64_t u64() const;
    float32_t f32() const;
    float64_t f64() const;
    char c() const;
    string_t s() const;
    ListPtr list() const;
    FieldPtr field() const;
    RecordPtr record() const;
    TuplePtr tuple() const;
    CasePtr case_() const;
    VariantPtr variant() const;
    EnumPtr enum_() const;
    OptionPtr option() const;
    ResultPtr result() const;
    // FlagsPtr flags() const;
    // OwnPtr own() const;
    // BorrowPtr borrow() const;
    // FuncType *func() const;
};

enum class WasmValType : uint8_t
{
    I32,
    I64,
    F32,
    F64
};

class WasmVal
{
    val_t val;

    WasmVal();
    WasmVal(val_t val);

public:
    WasmVal(uint32_t i32);
    WasmVal(uint64_t i64);
    WasmVal(float32_t f32);
    WasmVal(float64_t f64);

    WasmVal(const WasmVal &other);
    WasmVal(WasmVal &&other) noexcept;
    ~WasmVal();
    WasmVal &operator=(const WasmVal &other) noexcept;
    WasmVal &operator=(WasmVal &&other) noexcept;

    WasmValType kind() const;
    uint32_t i32() const;
    uint64_t i64() const;
    float32_t f32() const;
    float64_t f64() const;
};

// class FuncType
// {
// public:
//     virtual ~FuncType() = default;

//     virtual std::vector<ValType> param_types() = 0;
//     virtual std::vector<ValType> result_types() = 0;
// };
// FuncTypePtr createFuncType();

class List
{
public:
    const ValType &t;
    std::vector<Val> vs;

    List(const ValType &t);
    virtual ~List() = default;
};

class Field
{
public:
    const std::string &label;
    const ValType &t;
    std::optional<Val> v;

    Field(const std::string &label, const ValType &t);
    virtual ~Field() = default;
};

class Record
{
public:
    std::vector<Field> fields;

    Record();
    virtual ~Record() = default;
};

class Tuple
{
public:
    std::vector<Val> vs;

    Tuple();
    virtual ~Tuple() = default;
};

class Case
{
public:
    const std::string &label;
    std::optional<Val> v;
    std::optional<std::string> refines;

    Case(const std::string &label, const std::optional<Val> &v = std::nullopt, const std::optional<std::string> &refines = std::nullopt);
    virtual ~Case() = default;
};

class Variant
{
public:
    const std::vector<Case> &cases;

    Variant(const std::vector<Case> &cases);
    virtual ~Variant() = default;
};

class Enum
{
public:
    const std::vector<std::string> &labels;

    Enum(const std::vector<std::string> &labels);
    virtual ~Enum() = default;
};

class Option
{
public:
    const Val &v;

    Option(const Val &v);
    virtual ~Option() = default;
};

class Result
{
public:
    const std::optional<Val> &ok;
    const std::optional<Val> &error;

    Result(const std::optional<Val> &ok, const std::optional<Val> &error);
    virtual ~Result() = default;
};

class Flags
{
public:
    const std::vector<std::string> &labels;

    Flags(const std::vector<std::string> &labels);
    virtual ~Flags() = default;
};

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

std::vector<WasmVal> lower_flat(const CallContext &cx, const Val &v);
