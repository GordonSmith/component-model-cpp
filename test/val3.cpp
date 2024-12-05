#include "val3.hpp"

#include <string>

#include <doctest/doctest.h>

using namespace cmcpp3;

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
    Flags,
    Own,
    Borrow
};

struct Bool
{
    bool v;
};

struct String
{
    std::string v;
};

struct Person
{
    std::string lname;
    std::string rname;
    int age;
    std::vector<Person> children;
};

TEST_CASE("Val")
{
    // auto x = HostVal(true);
    // static_assert(std::is_same_v<decltype(x), cmcpp3::detail::reference<int>>, "Error");

    Bool b{true};
    String s{"Hello"};

    // std::cout << HostVal(b) << std::endl;
    // std::cout << HostVal(s) << std::endl;

    print(true);
    std::cout << std::endl;
    print((int8_t)22);
    std::cout << std::endl;
    print((uint8_t)22);
    std::cout << std::endl;
    print((int16_t)22);
    std::cout << std::endl;
    print((uint16_t)22);
    std::cout << std::endl;
    print((int32_t)22);
    std::cout << std::endl;
    print((uint32_t)22);
    std::cout << std::endl;

    print((int64_t)22);
    std::cout << std::endl;
    print((uint64_t)22);
    std::cout << std::endl;
    print((float32_t)22);
    std::cout << std::endl;
    print((float64_t)22);
    std::cout << std::endl;
    print('q');
    std::cout << std::endl;
    print("aaaabbb123");
    std::cout << std::endl;

    std::vector<bool> v1 = {true, false, true};
    print(v1);
    std::cout << std::endl;

    std::vector<std::vector<bool>> v2 = {{true, false, true}, {false, true, false}};
    print(v2);
    std::cout << std::endl;

    std::vector<std::vector<std::string>> v3 = {{"aaa", "bbb", "ccc"}, {"ddd", "eee", "fff"}};
    print(v3);
    std::cout << std::endl;

    Person p1{"John", "Doe", 22, {}};
    print(p1);
    std::cout << std::endl;

    std::vector<Person> v4 = {};
    print(v4);
    std::cout << std::endl;

    std::tuple<bool, int, std::string, Person, std::vector<Person>> t1;
    print(t1);
    std::cout << std::endl;
}

namespace cmcpp3
{

    // Val::Val() : val{}
    // {
    //     val.kind = ValType::U32;
    //     val.of.u32 = 0;
    // }
    // Val::Val(val_t val) : val(val) {}

    // Val::Val(bool b) : val{}
    // {
    //     val.kind = ValType::Bool;
    //     val.of.b = b;
    // }

    // Val::Val(int8_t s8) : val{}
    // {
    //     val.kind = ValType::S8;
    //     val.of.s8 = s8;
    // }

    // Val::Val(uint8_t u8) : val{}
    // {
    //     val.kind = ValType::U8;
    //     val.of.u8 = u8;
    // }

    // Val::Val(int16_t s16) : val{}
    // {
    //     val.kind = ValType::S16;
    //     val.of.s16 = s16;
    // }

    // Val::Val(uint16_t u16) : val{}
    // {
    //     val.kind = ValType::U16;
    //     val.of.u16 = u16;
    // }

    // Val::Val(int32_t s32) : val{}
    // {
    //     val.kind = ValType::S32;
    //     val.of.s32 = s32;
    // }

    // Val::Val(uint32_t u32) : val{}
    // {
    //     val.kind = ValType::U32;
    //     val.of.s32 = u32;
    // }

    // Val::Val(int64_t s64) : val{}
    // {
    //     val.kind = ValType::S64;
    //     val.of.s64 = s64;
    // }

    // Val::Val(uint64_t u64) : val{}
    // {
    //     val.kind = ValType::U64;
    //     val.of.u64 = u64;
    // }

    // Val::Val(float32_t f32) : val{}
    // {
    //     val.kind = ValType::Float32;
    //     val.of.f32 = f32;
    // }

    // Val::Val(float64_t f64) : val{}
    // {
    //     val.kind = ValType::Float64;
    //     val.of.f64 = f64;
    // }

    // Val::Val(char c) : val{}
    // {
    //     val.kind = ValType::Char;
    //     val.of.c = c;
    // }

    // Val::Val(const char *s) : val{}
    // {
    //     val.kind = ValType::String;
    //     val.of.s.ptr = (const char *)s;
    //     val.of.s.len = strlen(s);
    // }

    // Val::Val(const char *s, size_t len) : val{}
    // {
    //     val.kind = ValType::String;
    //     val.of.s.ptr = s;
    //     val.of.s.len = len;
    // }

    // Val::Val(ListPtr list) : val{}
    // {
    //     val.kind = ValType::List;
    //     val.of.list = list.get();
    //     shared_ptr = list;
    // }

    // Val::Val(FieldPtr field) : val{}
    // {
    //     val.kind = ValType::Field;
    //     val.of.field = field.get();
    //     shared_ptr = field;
    // }

    // Val::Val(RecordPtr record) : val{}
    // {
    //     val.kind = ValType::Record;
    //     val.of.record = record.get();
    //     shared_ptr = record;
    // }

    // Val::Val(TuplePtr tuple) : val{}
    // {
    //     val.kind = ValType::Tuple;
    //     val.of.tuple = tuple.get();
    //     shared_ptr = tuple;
    // }

    // Val::Val(CasePtr case_) : val{}
    // {
    //     val.kind = ValType::Case;
    //     val.of.case_ = case_.get();
    //     shared_ptr = case_;
    // }

    // Val::Val(VariantPtr variant) : val{}
    // {
    //     val.kind = ValType::Variant;
    //     val.of.variant = variant.get();
    //     shared_ptr = variant;
    // }

    // Val::Val(EnumPtr enum_) : val{}
    // {
    //     val.kind = ValType::Enum;
    //     val.of.enum_ = enum_.get();
    //     shared_ptr = enum_;
    // }

    // Val::Val(OptionPtr option) : val{}
    // {
    //     val.kind = ValType::Option;
    //     val.of.option = option.get();
    //     shared_ptr = option;
    // }

    // Val::Val(ResultPtr result) : val{}
    // {
    //     val.kind = ValType::Result;
    //     val.of.result = result.get();
    //     shared_ptr = result;
    // }

    // // Val::Val(FlagsPtr flags) : val{}
    // // {
    // //     val.kind = ValType::Flags;
    // //     val.of.flags = flags.get();
    // //     shared_ptr = flags;
    // // }

    // // Val::Val(OwnPtr own) : val{}
    // // {
    // //     val.kind = ValType::Own;
    // //     val.of.own = own.get();
    // //     shared_ptr = own;
    // // }

    // // Val::Val(BorrowPtr borrow) : val{}
    // // {
    // //     val.kind = ValType::Borrow;
    // //     val.of.borrow = borrow.get();
    // //     shared_ptr = borrow;
    // // }

    // // Val::Val(FuncTypePtr func) : val{}
    // // {
    // //     val.kind = ValType::Func;
    // //     val.of.func = func.get();
    // //     shared_ptr = func;
    // // }

    // Val::Val(const Val &other) : val{}
    // {
    //     val = other.val;
    //     shared_ptr = other.shared_ptr;
    // }

    // Val::Val(Val &&other) noexcept : val{}
    // {
    //     val.kind = ValType::U32;
    //     val.of.u32 = 0;
    //     std::swap(val, other.val);
    //     std::swap(shared_ptr, other.shared_ptr);
    // }

    // Val::~Val() {}

    // Val &Val::operator=(const Val &other) noexcept
    // {
    //     val = other.val;
    //     shared_ptr = other.shared_ptr;
    //     return *this;
    // }
    // Val &Val::operator=(Val &&other) noexcept
    // {
    //     std::swap(val, other.val);
    //     std::swap(shared_ptr, other.shared_ptr);
    //     return *this;
    // }

    // ValType Val::kind() const { return val.kind; }

    // bool Val::b() const
    // {
    //     if (val.kind != ValType::Bool)
    //         std::abort();
    //     return val.of.b;
    // }

    // int8_t Val::s8() const
    // {
    //     if (val.kind != ValType::S8)
    //         std::abort();
    //     return val.of.s8;
    // }

    // uint8_t Val::u8() const
    // {
    //     if (val.kind != ValType::U8)
    //         std::abort();
    //     return val.of.u8;
    // }

    // int16_t Val::s16() const
    // {
    //     if (val.kind != ValType::S16)
    //         std::abort();
    //     return val.of.s16;
    // }

    // uint16_t Val::u16() const
    // {
    //     if (val.kind != ValType::U16)
    //         std::abort();
    //     return val.of.u16;
    // }

    // int32_t Val::s32() const
    // {
    //     if (val.kind != ValType::S32)
    //         std::abort();
    //     return val.of.s32;
    // }

    // uint32_t Val::u32() const
    // {
    //     if (val.kind != ValType::U32)
    //         std::abort();
    //     return val.of.u32;
    // }

    // int64_t Val::s64() const
    // {
    //     if (val.kind != ValType::S64)
    //         std::abort();
    //     return val.of.s64;
    // }

    // uint64_t Val::u64() const
    // {
    //     if (val.kind != ValType::U64)
    //         std::abort();
    //     return val.of.u64;
    // }

    // float32_t Val::f32() const
    // {
    //     if (val.kind != ValType::Float32)
    //         std::abort();
    //     return val.of.f32;
    // }

    // float64_t Val::f64() const
    // {
    //     if (val.kind != ValType::Float64)
    //         std::abort();
    //     return val.of.f64;
    // }

    // char Val::c() const
    // {
    //     if (val.kind != ValType::Char)
    //         std::abort();
    //     return val.of.c;
    // }

    // utf8_t Val::s() const
    // {
    //     if (val.kind != ValType::String)
    //         std::abort();
    //     return val.of.s;
    // }

    // std::string Val::string() const
    // {
    //     if (val.kind != ValType::String)
    //         std::abort();
    //     return std::string((const char *)val.of.s.ptr, val.of.s.len);
    // }

    // ListPtr Val::list() const
    // {
    //     if (val.kind != ValType::List)
    //         std::abort();
    //     return std::get<ListPtr>(shared_ptr);
    // }

    // FieldPtr Val::field() const
    // {
    //     if (val.kind != ValType::Field)
    //         std::abort();
    //     return std::get<FieldPtr>(shared_ptr);
    // }

    // RecordPtr Val::record() const
    // {
    //     if (val.kind != ValType::Record)
    //         std::abort();
    //     return std::get<RecordPtr>(shared_ptr);
    // }

    // TuplePtr Val::tuple() const
    // {
    //     if (val.kind != ValType::Tuple)
    //         std::abort();
    //     return std::get<TuplePtr>(shared_ptr);
    // }

    // CasePtr Val::case_() const
    // {
    //     if (val.kind != ValType::Case)
    //         std::abort();
    //     return std::get<CasePtr>(shared_ptr);
    // }

    // VariantPtr Val::variant() const
    // {
    //     if (val.kind != ValType::Variant)
    //         std::abort();
    //     return std::get<VariantPtr>(shared_ptr);
    // }

    // EnumPtr Val::enum_() const
    // {
    //     if (val.kind != ValType::Enum)
    //         std::abort();
    //     return std::get<EnumPtr>(shared_ptr);
    // }

    // OptionPtr Val::option() const
    // {
    //     if (val.kind != ValType::Option)
    //         std::abort();
    //     return std::get<OptionPtr>(shared_ptr);
    // }

    // ResultPtr Val::result() const
    // {
    //     if (val.kind != ValType::Result)
    //         std::abort();
    //     return std::get<ResultPtr>(shared_ptr);
    // }

    // FlagsPtr Val::flags() const
    // {
    //     if (val.kind != ValType::Flags)
    //         std::abort();
    //     return std::get<FlagsPtr>(shared_ptr);
    // }

    // OwnPtr Val::own() const
    // {
    //     if (val.kind != ValType::Own)
    //         std::abort();
    //     return std::get<OwnPtr>(shared_ptr);
    // }

    // BorrowPtr Val::borrow() const
    // {
    //     if (val.kind != ValType::Borrow)
    //         std::abort();
    //     return std::get<BorrowPtr>(shared_ptr);
    // }

    // FuncType *Val::func() const
    // {
    //     if (val.kind != ValType::Borrow)
    //         std::abort();
    //     return val.of.func;
    // }

    // WasmVal::WasmVal() {}

    // WasmVal::WasmVal(val_t val) : val(val) {}

    // WasmVal::WasmVal(int32_t i32) : val{}
    // {
    //     val.kind = ValType::S32;
    //     val.of.s32 = i32;
    // }

    // WasmVal::WasmVal(uint32_t i32) : val{}
    // {
    //     val.kind = ValType::S32;
    //     val.of.s32 = i32;
    // }

    // WasmVal::WasmVal(int64_t i64) : val{}
    // {
    //     val.kind = ValType::S64;
    //     val.of.s64 = i64;
    // }

    // WasmVal::WasmVal(uint64_t i64) : val{}
    // {
    //     val.kind = ValType::S64;
    //     val.of.s64 = i64;
    // }

    // WasmVal::WasmVal(float32_t f32) : val{}
    // {
    //     val.kind = ValType::Float32;
    //     val.of.f32 = f32;
    // }

    // WasmVal::WasmVal(float64_t f64) : val{}
    // {
    //     val.kind = ValType::Float64;
    //     val.of.f64 = f64;
    // }

    // WasmVal::WasmVal(const WasmVal &other) : val{} { val = other.val; }

    // WasmVal::WasmVal(WasmVal &&other) noexcept : val{}
    // {
    //     val.kind = ValType::S32;
    //     val.of.s32 = 0;
    //     std::swap(val, other.val);
    // }

    // WasmVal::~WasmVal()
    // {
    //     // if (val.kind == WASMTIME_EXTERNREF && val.of.externref != nullptr)
    //     // {
    //     //      TODO:  wasmtime_externref_delete(val.of.externref);
    //     // }
    // }

    // /// Copies the contents of another value into this one.
    // WasmVal &WasmVal::operator=(const WasmVal &other) noexcept
    // {
    //     val = other.val;
    //     return *this;
    // }

    // WasmVal &WasmVal::operator=(WasmVal &&other) noexcept
    // {
    //     std::swap(val, other.val);
    //     return *this;
    // }

    // WasmValType WasmVal::kind() const
    // {
    //     switch (val.kind)
    //     {
    //     case ValType::S32:
    //         return WasmValType::I32;
    //     case ValType::S64:
    //         return WasmValType::I64;
    //     case ValType::Float32:
    //         return WasmValType::F32;
    //     case ValType::Float64:
    //         return WasmValType::F64;
    //     default:
    //         throw std::runtime_error("Invalid WasmValType");
    //     }
    // }

    // int32_t WasmVal::i32() const
    // {
    //     if (val.kind != ValType::S32)
    //         std::abort();
    //     return val.of.s32;
    // }

    // int64_t WasmVal::i64() const
    // {
    //     if (val.kind != ValType::S64)
    //         std::abort();
    //     return val.of.s64;
    // }

    // float32_t WasmVal::f32() const
    // {
    //     if (val.kind != ValType::Float32)
    //         std::abort();
    //     return val.of.f32;
    // }

    // float64_t WasmVal::f64() const
    // {
    //     if (val.kind != ValType::Float64)
    //         std::abort();
    //     return val.of.f64;
    // }

    // List::List(const ValType &t) : t(t) {}

    // Field::Field(const std::string &label, const ValType &t) : label(label), t(t) {}

    // Record::Record() {}

    // Tuple::Tuple() {}

    // Case::Case(const std::string &label, const std::optional<Val> &v,
    //            const std::optional<std::string> &refines)
    //     : label(label), v(v), refines(refines) {}

    // Variant::Variant(const std::vector<Case> &cases) : cases(cases) {}

    // Enum::Enum(const std::vector<std::string> &labels) : labels(labels) {}

    // Option::Option(const Val &v) : v(v) {}

    // Result::Result(const std::optional<Val> &ok, const std::optional<Val> &error)
    //     : ok(ok), error(error) {}

    // Flags::Flags(const std::vector<std::string> &labels) : labels(labels) {}

    // using Param = std::pair<std::string, ValType>;
    // class FuncTypeImpl : public FuncType
    // {
    // protected:
    //     std::vector<Param> params;
    //     std::vector<ValType> results;

    // public:
    //     FuncTypeImpl()
    //     {
    //     }
    //     std::vector<ValType> param_types()
    //     {
    //         std::vector<ValType> retVal;
    //         if (!params.empty())
    //         {
    //             for (const auto &pair : params)
    //             {
    //                 retVal.push_back(pair.second);
    //             }
    //         }
    //         return retVal;
    //     }

    //     std::vector<ValType> result_types()
    //     {
    //         return results;
    //     }
    // };

    // FuncTypePtr createFuncType()
    // {
    //     return std::make_shared<FuncTypeImpl>();
    // }

}