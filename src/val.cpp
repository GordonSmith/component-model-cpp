#include "val.hpp"

#include <any>
#include <cassert>
#include <cmath>
#include <cstring>
#include <map>
#include <random>
#include <sstream>

Val::Val() : val{} {
  val.kind = ValType::U32;
  val.of.u32 = 0;
}
Val::Val(val_t val) : val(val) {}

Val::Val(bool b) : val{} {
  val.kind = ValType::Bool;
  val.of.b = b;
}

Val::Val(int8_t s8) : val{} {
  val.kind = ValType::S8;
  val.of.s8 = s8;
}

Val::Val(uint8_t u8) : val{} {
  val.kind = ValType::U8;
  val.of.u8 = u8;
}

Val::Val(int16_t s16) : val{} {
  val.kind = ValType::S16;
  val.of.s16 = s16;
}

Val::Val(uint16_t u16) : val{} {
  val.kind = ValType::U16;
  val.of.u16 = u16;
}

Val::Val(int32_t s32) : val{} {
  val.kind = ValType::S32;
  val.of.s32 = s32;
}

Val::Val(uint32_t u32) : val{} {
  val.kind = ValType::U32;
  val.of.s32 = u32;
}

Val::Val(int64_t s64) : val{} {
  val.kind = ValType::S64;
  val.of.s64 = s64;
}

Val::Val(uint64_t u64) : val{} {
  val.kind = ValType::U64;
  val.of.u64 = u64;
}

Val::Val(float32_t f32) : val{} {
  val.kind = ValType::Float32;
  val.of.f32 = f32;
}

Val::Val(float64_t f64) : val{} {
  val.kind = ValType::Float64;
  val.of.f64 = f64;
}

Val::Val(char c) : val{} {
  val.kind = ValType::Char;
  val.of.c = c;
}

Val::Val(const char *s) : val{} {
  val.kind = ValType::String;
  val.of.s.ptr = s;
  val.of.s.len = strlen(s);
}

Val::Val(const char *s, size_t len) : val{} {
  val.kind = ValType::String;
  val.of.s.ptr = s;
  val.of.s.len = len;
}

Val::Val(ListPtr list) : val{} {
  val.kind = ValType::List;
  val.of.list = list.get();
  shared_ptr = list;
}

Val::Val(FieldPtr field) : val{} {
  val.kind = ValType::Field;
  val.of.field = field.get();
  shared_ptr = field;
}

Val::Val(RecordPtr record) : val{} {
  val.kind = ValType::Record;
  val.of.record = record.get();
  shared_ptr = record;
}

Val::Val(TuplePtr tuple) : val{} {
  val.kind = ValType::Tuple;
  val.of.tuple = tuple.get();
  shared_ptr = tuple;
}

Val::Val(CasePtr case_) : val{} {
  val.kind = ValType::Case;
  val.of.case_ = case_.get();
  shared_ptr = case_;
}

Val::Val(VariantPtr variant) : val{} {
  val.kind = ValType::Variant;
  val.of.variant = variant.get();
  shared_ptr = variant;
}

Val::Val(EnumPtr enum_) : val{} {
  val.kind = ValType::Enum;
  val.of.enum_ = enum_.get();
  shared_ptr = enum_;
}

Val::Val(OptionPtr option) : val{} {
  val.kind = ValType::Option;
  val.of.option = option.get();
  shared_ptr = option;
}

Val::Val(ResultPtr result) : val{} {
  val.kind = ValType::Result;
  val.of.result = result.get();
  shared_ptr = result;
}

// Val::Val(FlagsPtr flags) : val{}
// {
//     val.kind = ValType::Flags;
//     val.of.flags = flags.get();
//     shared_ptr = flags;
// }

// Val::Val(OwnPtr own) : val{}
// {
//     val.kind = ValType::Own;
//     val.of.own = own.get();
//     shared_ptr = own;
// }

// Val::Val(BorrowPtr borrow) : val{}
// {
//     val.kind = ValType::Borrow;
//     val.of.borrow = borrow.get();
//     shared_ptr = borrow;
// }

// Val::Val(FuncTypePtr func) : val{}
// {
//     val.kind = ValType::Func;
//     val.of.func = func.get();
//     shared_ptr = func;
// }

Val::Val(const Val &other) : val{} {
  val = other.val;
  shared_ptr = other.shared_ptr;
}

Val::Val(Val &&other) noexcept : val{} {
  val.kind = ValType::U32;
  val.of.u32 = 0;
  std::swap(val, other.val);
  std::swap(shared_ptr, other.shared_ptr);
}

Val::~Val() {}

Val &Val::operator=(const Val &other) noexcept {
  val = other.val;
  shared_ptr = other.shared_ptr;
  return *this;
}
Val &Val::operator=(Val &&other) noexcept {
  std::swap(val, other.val);
  std::swap(shared_ptr, other.shared_ptr);
  return *this;
}

ValType Val::kind() const { return val.kind; }

bool Val::b() const {
  if (val.kind != ValType::Bool) std::abort();
  return val.of.b;
}

int8_t Val::s8() const {
  if (val.kind != ValType::S8) std::abort();
  return val.of.s8;
}

uint8_t Val::u8() const {
  if (val.kind != ValType::U8) std::abort();
  return val.of.u8;
}

int16_t Val::s16() const {
  if (val.kind != ValType::S16) std::abort();
  return val.of.s16;
}

uint16_t Val::u16() const {
  if (val.kind != ValType::U16) std::abort();
  return val.of.u16;
}

int32_t Val::s32() const {
  if (val.kind != ValType::S32) std::abort();
  return val.of.s32;
}

uint32_t Val::u32() const {
  if (val.kind != ValType::U32) std::abort();
  return val.of.u32;
}

int64_t Val::s64() const {
  if (val.kind != ValType::S64) std::abort();
  return val.of.s64;
}

uint64_t Val::u64() const {
  if (val.kind != ValType::U64) std::abort();
  return val.of.u64;
}

float32_t Val::f32() const {
  if (val.kind != ValType::Float32) std::abort();
  return val.of.f32;
}

float64_t Val::f64() const {
  if (val.kind != ValType::Float64) std::abort();
  return val.of.f64;
}

char Val::c() const {
  if (val.kind != ValType::Char) std::abort();
  return val.of.c;
}

string_t Val::s() const {
  if (val.kind != ValType::String) std::abort();
  return val.of.s;
}

ListPtr Val::list() const {
  if (val.kind != ValType::List) std::abort();
  return std::get<ListPtr>(shared_ptr);
}

FieldPtr Val::field() const {
  if (val.kind != ValType::Field) std::abort();
  return std::get<FieldPtr>(shared_ptr);
}

RecordPtr Val::record() const {
  if (val.kind != ValType::Record) std::abort();
  return std::get<RecordPtr>(shared_ptr);
}

TuplePtr Val::tuple() const {
  if (val.kind != ValType::Tuple) std::abort();
  return std::get<TuplePtr>(shared_ptr);
}

CasePtr Val::case_() const {
  if (val.kind != ValType::Case) std::abort();
  return std::get<CasePtr>(shared_ptr);
}

VariantPtr Val::variant() const {
  if (val.kind != ValType::Variant) std::abort();
  return std::get<VariantPtr>(shared_ptr);
}

EnumPtr Val::enum_() const {
  if (val.kind != ValType::Enum) std::abort();
  return std::get<EnumPtr>(shared_ptr);
}

OptionPtr Val::option() const {
  if (val.kind != ValType::Option) std::abort();
  return std::get<OptionPtr>(shared_ptr);
}

ResultPtr Val::result() const {
  if (val.kind != ValType::Result) std::abort();
  return std::get<ResultPtr>(shared_ptr);
}

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

WasmVal::WasmVal() : val{} {}

WasmVal::WasmVal(val_t val) : val(val) {}

WasmVal::WasmVal(uint32_t i32) : val{} {
  val.kind = ValType::U32;
  val.of.u32 = i32;
}

WasmVal::WasmVal(uint64_t i64) : val{} {
  val.kind = ValType::U64;
  val.of.u64 = i64;
}

WasmVal::WasmVal(float32_t f32) : val{} {
  val.kind = ValType::Float32;
  val.of.f32 = f32;
}

WasmVal::WasmVal(float64_t f64) : val{} {
  val.kind = ValType::Float64;
  val.of.f64 = f64;
}

WasmVal::WasmVal(const WasmVal &other) : val{} { val = other.val; }

WasmVal::WasmVal(WasmVal &&other) noexcept : val{} {
  val.kind = ValType::U32;
  val.of.u32 = 0;
  std::swap(val, other.val);
}

WasmVal::~WasmVal() {
  // if (val.kind == WASMTIME_EXTERNREF && val.of.externref != nullptr)
  // {
  //      TODO:  wasmtime_externref_delete(val.of.externref);
  // }
}

/// Copies the contents of another value into this one.
WasmVal &WasmVal::operator=(const WasmVal &other) noexcept {
  // if (val.kind == WASMTIME_EXTERNREF && val.of.externref != nullptr)
  // {
  //     // TODO:  wasmtime_externref_delete(val.of.externref);
  // }
  // // TODO: val_copy(&val, &other.val);
  val = other.val;
  return *this;
}

WasmVal &WasmVal::operator=(WasmVal &&other) noexcept {
  std::swap(val, other.val);
  return *this;
}

WasmValType WasmVal::kind() const {
  switch (val.kind) {
    case ValType::U32:
      return WasmValType::I32;
    case ValType::U64:
      return WasmValType::I64;
    case ValType::Float32:
      return WasmValType::F32;
    case ValType::Float64:
      return WasmValType::F64;
    default:
      return WasmValType::I32;
  }
}

uint32_t WasmVal::i32() const {
  if (val.kind != ValType::U32) std::abort();
  return val.of.u32;
}

uint64_t WasmVal::i64() const {
  if (val.kind != ValType::U64) std::abort();
  return val.of.u64;
}

float32_t WasmVal::f32() const {
  if (val.kind != ValType::Float32) std::abort();
  return val.of.f32;
}

float64_t WasmVal::f64() const {
  if (val.kind != ValType::Float64) std::abort();
  return val.of.f64;
}

List::List(const ValType &t) : t(t) {}

Field::Field(const std::string &label, const ValType &t) : label(label), t(t) {}

Record::Record() {}

Tuple::Tuple() {}

Case::Case(const std::string &label, const std::optional<Val> &v,
           const std::optional<std::string> &refines)
    : label(label), v(v), refines(refines) {}

Variant::Variant(const std::vector<Case> &cases) : cases(cases) {}

Enum::Enum(const std::vector<std::string> &labels) : labels(labels) {}

Option::Option(const Val &v) : v(v) {}

Result::Result(const std::optional<Val> &ok, const std::optional<Val> &error)
    : ok(ok), error(error) {}

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

CoreFuncType::CoreFuncType(const std::vector<std::string> &params,
                           const std::vector<std::string> &results)
    : params(params), results(results) {}

//  ABI       ----------------------------------------------------------------

Val despecialize(const Val &v) {
  switch (v.kind()) {
    case ValType::Tuple: {
      RecordPtr r = std::make_shared<Record>();
      for (const auto &v2 : v.tuple()->vs) {
        Field f("", despecialize(v).kind());
        f.v = v2;
        r->fields.push_back(f);
      }
      return r;
    }
    case ValType::Enum: {
      std::vector<Case> cases;
      for (const auto &label : v.enum_()->labels) {
        cases.push_back(Case(label));
      }
      VariantPtr v = std::make_shared<Variant>(cases);
      return v;
    }
    case ValType::Option:
      return std::make_shared<Variant>(
          std::vector<Case>{Case("None"), Case("Some", v.option()->v)});
    case ValType::Result:
      return std::make_shared<Variant>(
          std::vector<Case>{Case("Ok", v.result()->ok), Case("Error", v.result()->error)});
    default:
      return v;
  }
}

ValType discriminant_type(const std::vector<Case> &cases) {
  size_t n = cases.size();

  assert(0 < n && n < std::numeric_limits<unsigned int>::max());
  int match = std::ceil(std::log2(n) / 8);
  switch (match) {
    case 0:
      return ValType::U8;
    case 1:
      return ValType::U8;
    case 2:
      return ValType::U16;
    case 3:
      return ValType::U32;
    default:
      throw std::runtime_error("Invalid match value");
  }
}

int alignment_record(const std::vector<Field> &fields);
int alignment_variant(const std::vector<Case> &cases);

int alignment_flags(const std::vector<std::string> &labels) {
  int n = labels.size();
  if (n <= 8) return 1;
  if (n <= 16) return 2;
  return 4;
}

int alignment(ValType t) {
  switch (t) {
    case ValType::Bool:
    case ValType::S8:
    case ValType::U8:
      return 1;
    case ValType::S16:
    case ValType::U16:
      return 2;
    case ValType::S32:
    case ValType::U32:
    case ValType::Float32:
    case ValType::Char:
      return 4;
    case ValType::S64:
    case ValType::U64:
    case ValType::Float64:
      return 8;
    case ValType::String:
    case ValType::List:
      return 4;
      // case ValType::Own:
      // case ValType::Borrow:
      //     return 4;
    default:
      throw std::runtime_error("Invalid type");
  }
}

int alignment(Val _v) {
  Val v = despecialize(_v);
  switch (v.kind()) {
    case ValType::Record:
      return alignment_record(_v.record()->fields);
    case ValType::Variant:
      return alignment_variant(_v.variant()->cases);
    // case ValType::Flags:
    //     return alignment_flags(_v.flags()->labels);
    default:
      return alignment(v);
  }
}

int alignment_record(const std::vector<Field> &fields) {
  int a = 1;
  for (const auto &f : fields) {
    a = std::max(a, alignment(f.v.value()));
  }
  return a;
}

int max_case_alignment(const std::vector<Case> &cases) {
  int a = 1;
  for (const auto &c : cases) {
    if (c.v.has_value())  // Check if c.t exists
    {
      a = std::max(a, alignment(c.v.value()));
    }
  }
  return a;
}

int alignment_variant(const std::vector<Case> &cases) {
  return std::max(alignment(discriminant_type(cases)), max_case_alignment(cases));
}

uint32_t align_to(uint32_t ptr, uint32_t alignment);

int size_record(const std::vector<Field> &fields);
int size_variant(const std::vector<Case> &cases);
int size_flags(const std::vector<std::string> &labels);

int size(ValType t) {
  switch (t) {
    case ValType::Bool:
    case ValType::S8:
    case ValType::U8:
      return 1;
    case ValType::S16:
    case ValType::U16:
      return 2;
    case ValType::S32:
    case ValType::U32:
    case ValType::Float32:
    case ValType::Char:
      return 4;
    case ValType::S64:
    case ValType::U64:
    case ValType::Float64:
      return 8;
    case ValType::String:
    case ValType::List:
      return 8;
      // case ValType::Own:
      // case ValType::Borrow:
      //     return 4;
    default:
      throw std::runtime_error("Invalid type");
  }
}

int size(const Val &v) {
  ValType kind = despecialize(v).kind();
  switch (kind) {
    case ValType::Record:
      return size_record(v.record()->fields);
    case ValType::Variant:
      return size_variant(v.variant()->cases);
    // case ValType::Flags:
    //     return size_flags(v.flags()->labels);
    default:
      return size(kind);
  }
  throw std::runtime_error("Invalid type");
}

int size_record(const std::vector<Field> &fields) {
  int s = 0;
  for (const auto &f : fields) {
    s = align_to(s, alignment(f.v.value()));
    s += size(f.v.value());
  }
  assert(s > 0);
  return align_to(s, alignment_record(fields));
}

uint32_t align_to(uint32_t ptr, uint32_t alignment) {
  return (ptr + alignment - 1) & ~(alignment - 1);
}

int size_variant(const std::vector<Case> &cases) {
  int s = size(discriminant_type(cases));
  s = align_to(s, max_case_alignment(cases));
  int cs = 0;
  for (const auto &c : cases) {
    if (c.v.has_value()) {
      cs = std::max(cs, size(c.v.value()));
    }
  }
  s += cs;
  return align_to(s, alignment_variant(cases));
}

int num_i32_flags(const std::vector<std::string> &labels);

int size_flags(const std::vector<std::string> &labels) {
  int n = labels.size();
  assert(n > 0);
  if (n <= 8) return 1;
  if (n <= 16) return 2;
  return 4 * num_i32_flags(labels);
}

int num_i32_flags(const std::vector<std::string> &labels) {
  return std::ceil(static_cast<double>(labels.size()) / 32);
}

bool isAligned(uint32_t ptr, uint32_t alignment) { return (ptr & (alignment - 1)) == 0; }

//  loading
//  --------------------------------------------------------------------------------------------

Val load(const CallContext &cx, uint32_t ptr, ValType t);
Val load(const CallContext &cx, uint32_t ptr, ValType t, ValType lt);
Val load(const CallContext &cx, uint32_t ptr, ValType t, const std::vector<Field> &fields);
Val load(const CallContext &cx, uint32_t ptr, ValType t, const std::vector<Case> &cases);

template <typename T> T load_int(const CallContext &cx, uint32_t ptr, uint8_t) {
  T retVal = 0;
  for (size_t i = 0; i < sizeof(T); ++i) {
    retVal |= static_cast<T>(cx.opts->memory[ptr + i]) << (8 * i);
  }
  return retVal;
}

bool convert_int_to_bool(uint8_t i) { return i > 0; }

bool DETERMINISTIC_PROFILE = false;
float32_t CANONICAL_FLOAT32_NAN = 0x7fc00000;
float64_t CANONICAL_FLOAT64_NAN = 0x7ff8000000000000;

float canonicalize_nan32(float32_t f) {
  if (std::isnan(f)) {
    f = CANONICAL_FLOAT32_NAN;
    assert(std::isnan(f));
  }
  return f;
}

float64_t canonicalize_nan64(float64_t f) {
  if (std::isnan(f)) {
    f = CANONICAL_FLOAT64_NAN;
    assert(std::isnan(f));
  }
  return f;
}

float core_f32_reinterpret_i32(int32_t i);
float decode_i32_as_float(int32_t i) { return canonicalize_nan32(core_f32_reinterpret_i32(i)); }

double core_f64_reinterpret_i64(int64_t i);
double decode_i64_as_float(int64_t i) { return canonicalize_nan64(core_f64_reinterpret_i64(i)); }

float32_t core_f32_reinterpret_i32(int32_t i) {
  float f;
  std::memcpy(&f, &i, sizeof f);
  return f;
}

float64_t core_f64_reinterpret_i64(int64_t i) {
  double d;
  std::memcpy(&d, &i, sizeof d);
  return d;
}

char convert_i32_to_char(int32_t i) {
  assert(i < 0x110000);
  assert(!(0xD800 <= i && i <= 0xDFFF));
  return static_cast<char>(i);
}

using HostStringTuple
    = std::tuple<const char * /*s*/, std::string /*encoding*/, size_t /*byte length*/>;

HostStringTuple load_string_from_range(const CallContext &cx, uint32_t ptr,
                                       uint32_t tagged_code_units);
HostStringTuple load_string(const CallContext &cx, uint32_t ptr) {
  uint32_t begin = load_int<uint32_t>(cx, ptr, 4);
  uint32_t tagged_code_units = load_int<uint32_t>(cx, ptr + 4, 4);
  return load_string_from_range(cx, begin, tagged_code_units);
}

auto UTF16_TAG = 1U << 31;
HostStringTuple load_string_from_range(const CallContext &cx, uint32_t ptr,
                                       uint32_t tagged_code_units) {
  std::string encoding = "utf-8";
  uint32_t byte_length = tagged_code_units;
  uint32_t alignment = 1;
  if (cx.opts->string_encoding == HostEncoding::Utf8) {
    alignment = 1;
    byte_length = tagged_code_units;
    encoding = "utf-8";
  } else if (cx.opts->string_encoding == HostEncoding::Utf16) {
    alignment = 2;
    byte_length = 2 * tagged_code_units;
    encoding = "utf-16-le";
  } else if (cx.opts->string_encoding == HostEncoding::Latin1_Utf16) {
    alignment = 2;
    if (tagged_code_units & UTF16_TAG) {
      byte_length = 2 * (tagged_code_units ^ UTF16_TAG);
      encoding = "utf-16-le";
    } else {
      byte_length = tagged_code_units;
      encoding = "latin-1";
    }
  }
  assert(isAligned(ptr, alignment));
  assert(ptr + byte_length <= cx.opts->memory.size());
  // TODO decode the string
  // auto s = cx.opts->memory[ptr:ptr+byte_length].decode(encoding);
  std::string s(cx.opts->memory[ptr], byte_length);

  return HostStringTuple((const char *)&cx.opts->memory[ptr], encoding, byte_length);
}

ListPtr load_list_from_range(const CallContext &cx, uint32_t ptr, uint32_t length, ValType t);

ListPtr load_list(const CallContext &cx, uint32_t ptr, ValType t) {
  uint32_t begin = load_int<uint32_t>(cx, ptr, 4);
  uint32_t length = load_int<uint32_t>(cx, ptr + 4, 4);
  return load_list_from_range(cx, begin, length, t);
}

ListPtr load_list_from_range(const CallContext &cx, uint32_t ptr, uint32_t length, ValType t) {
  assert(ptr == align_to(ptr, alignment(t)));
  assert(ptr + length * size(t) <= cx.opts->memory.size());
  auto list = std::make_shared<List>(t);
  for (uint32_t i = 0; i < length; ++i) {
    list->vs.push_back(load(cx, ptr + i * size(t), t));
  }
  return list;
}

RecordPtr load_record(const CallContext &cx, uint32_t ptr, const std::vector<Field> &fields) {
  RecordPtr retVal = std::make_shared<Record>();
  for (auto &field : fields) {
    ptr = align_to(ptr, alignment(field.t));
    Field f(field.label, field.t);
    f.v = load(cx, ptr, field.t);
    retVal->fields.push_back(f);
    ptr += size(f.t);
  }
  return retVal;
}

std::string case_label_with_refinements(const Case &c, const std::vector<Case> &cases);
VariantPtr load_variant(const CallContext &cx, uint32_t ptr, const std::vector<Case> &cases) {
  uint32_t disc_size = size(discriminant_type(cases));
  uint32_t case_index = load_int<uint32_t>(cx, ptr, disc_size);
  ptr += disc_size;
  if (case_index >= cases.size()) {
    throw std::runtime_error("case_index out of range");
  }
  const Case &c = cases[case_index];
  ptr = align_to(ptr, max_case_alignment(cases));
  std::string case_label = case_label_with_refinements(c, cases);
  if (!c.v.has_value()) {
    Case c2(case_label, nullptr, c.refines);
    std::vector<Case> cases2 = {c2};
    return std::make_shared<Variant>(cases2);
  } else {
    Case c2(case_label, load(cx, ptr, c.v.value().kind()), c.refines);
    std::vector<Case> cases2 = {c2};
    return std::make_shared<Variant>(cases2);
  }
}

int find_case(const std::string &label, const std::vector<Case> &cases);
std::string case_label_with_refinements(const Case &c, const std::vector<Case> &) {
  std::string label = c.label;
  Case currentCase = c;

  while (currentCase.refines.has_value()) {
    // TODO:  currentCase = cases[find_case(currentCase.refines.value(), cases)];
    label += '|' + currentCase.label;
  }

  return label;
}

int find_case(const std::string &label, const std::vector<Case> &cases) {
  auto it = std::find_if(cases.begin(), cases.end(),
                         [&label](const Case &c) { return c.label == label; });

  if (it != cases.end()) {
    return std::distance(cases.begin(), it);
  }

  return -1;
}

std::map<std::string, bool> unpack_flags_from_int(int i, const std::vector<std::string> &labels);
std::map<std::string, bool> load_flags(const CallContext &cx, uint32_t ptr,
                                       const std::vector<std::string> &labels) {
  uint32_t i = load_int<uint32_t>(cx, ptr, size_flags(labels));
  return unpack_flags_from_int(i, labels);
}

std::map<std::string, bool> unpack_flags_from_int(int i, const std::vector<std::string> &labels) {
  std::map<std::string, bool> record;
  for (const auto &l : labels) {
    record[l] = bool(i & 1);
    i >>= 1;
  }
  return record;
}

// uint32_t lift_own(const CallContext &_cx, int i, const Own &t)
// {
//     CallContext &cx = const_cast<CallContext &>(_cx);
//     HandleElem h = cx.inst.handles.remove(t.rt, i);
//     if (h.lend_count != 0)
//     {
//         throw std::runtime_error("Lend count is not zero");
//     }
//     if (!h.own)
//     {
//         throw std::runtime_error("Not owning");
//     }
//     return h.rep;
// }

// uint32_t lift_borrow(const CallContext &_cx, int i, const Borrow &t)
// {
//     CallContext &cx = const_cast<CallContext &>(_cx);
//     HandleElem h = cx.inst.handles.get(t.rt, i);
//     if (h.own)
//     {
//         cx.track_owning_lend(h);
//     }
//     return h.rep;
// }

Val load(const CallContext &cx, uint32_t ptr, ValType t) {
  switch (t) {
    case ValType::Bool:
      return convert_int_to_bool(load_int<uint8_t>(cx, ptr, 1));
    case ValType::U8:
      return load_int<uint8_t>(cx, ptr, 1);
    case ValType::U16:
      return load_int<uint16_t>(cx, ptr, 2);
    case ValType::U32:
      return load_int<uint32_t>(cx, ptr, 4);
    case ValType::U64:
      return load_int<uint64_t>(cx, ptr, 8);
    case ValType::S8:
      return load_int<int8_t>(cx, ptr, 1);
    case ValType::S16:
      return load_int<int16_t>(cx, ptr, 2);
    case ValType::S32:
      return load_int<int32_t>(cx, ptr, 4);
    case ValType::S64:
      return load_int<int64_t>(cx, ptr, 8);
    case ValType::Float32:
      return decode_i32_as_float(load_int<int32_t>(cx, ptr, 4));
    case ValType::Float64:
      return decode_i64_as_float(load_int<int64_t>(cx, ptr, 8));
    case ValType::Char:
      return convert_i32_to_char(load_int<int32_t>(cx, ptr, 4));
    case ValType::String: {
      auto s = load_string(cx, ptr);
      return Val(std::get<0>(s), std::get<2>(s));
    }
      // case ValType::Flags:
      //     return load_flags(cx, ptr, std::get<3>(opt));
      // case ValType::Own:
      //     return lift_own(cx, load_int<uint32_t>(cx, ptr, 4), static_cast<const Own &>(t));
      // case ValType::Borrow:
      //     return lift_borrow(cx, load_int<uint32_t>(cx, ptr, 4), static_cast<const Borrow &>(t));
    default:
      throw std::runtime_error("Invalid type");
  }
}
Val load(const CallContext &cx, uint32_t ptr, ValType t, ValType lt) {
  switch (t) {
    case ValType::List:
      return load_list(cx, ptr, lt);
    default:
      throw std::runtime_error("Invalid type");
  }
}
Val load(const CallContext &cx, uint32_t ptr, ValType t, const std::vector<Field> &fields) {
  switch (t) {
    case ValType::Record:
      return load_record(cx, ptr, fields);
    default:
      throw std::runtime_error("Invalid type");
  }
}
Val load(const CallContext &cx, uint32_t ptr, ValType t, const std::vector<Case> &cases) {
  switch (t) {
    case ValType::Variant:
      return load_variant(cx, ptr, cases);
    default:
      throw std::runtime_error("Invalid type");
  }
}
//  Storing ----------------------------------------------------------------

uint32_t char_to_i32(char c) { return static_cast<uint32_t>(c); }

std::tuple<uint32_t, uint32_t> store_string_copy(const CallContext &cx, const char *_src,
                                                 uint32_t src_code_units,
                                                 uint32_t dst_code_unit_size,
                                                 uint32_t dst_alignment,
                                                 const std::string & /* dst_encoding*/) {
  std::string src(_src, src_code_units);
  const uint32_t MAX_STRING_BYTE_LENGTH = (1U << 31) - 1;
  uint32_t dst_byte_length = dst_code_unit_size * src_code_units;
  assert(dst_byte_length <= MAX_STRING_BYTE_LENGTH);
  uint32_t ptr = cx.opts->realloc(0, 0, dst_alignment, dst_byte_length);
  assert(ptr == align_to(ptr, dst_alignment));
  assert(ptr + dst_byte_length <= cx.opts->memory.size());
  // TODO:    std::string encoded = encode(src, dst_encoding);
  // TODO:    assert(dst_byte_length == encoded.size());
  std::string encoded = src;
  // assert(dst_byte_length == encoded.size());
  std::memcpy(&cx.opts->memory[ptr], encoded.data(), encoded.size());
  return std::make_tuple(ptr, src_code_units);
}

auto MAX_STRING_BYTE_LENGTH = (1U << 31) - 1;

std::tuple<uint32_t, uint32_t> store_string_to_utf8(const CallContext &cx, const char *src,
                                                    uint32_t src_code_units,
                                                    uint32_t worst_case_size) {
  assert(src_code_units <= MAX_STRING_BYTE_LENGTH);
  uint32_t ptr = cx.opts->realloc(0, 0, 1, src_code_units);
  assert(ptr + src_code_units <= cx.opts->memory.size());
  //  TODO:  std::string encoded = encode(src, "utf-8");
  std::string encoded = std::string(src, src_code_units);
  assert(src_code_units <= encoded.size());
  std::memcpy(&cx.opts->memory[ptr], encoded.data(), src_code_units);
  if (src_code_units < encoded.size()) {
    assert(worst_case_size <= MAX_STRING_BYTE_LENGTH);
    ptr = cx.opts->realloc(ptr, src_code_units, 1, worst_case_size);
    assert(ptr + worst_case_size <= cx.opts->memory.size());
    std::memcpy(&cx.opts->memory[ptr + src_code_units], &encoded[src_code_units],
                encoded.size() - src_code_units);
    if (worst_case_size > encoded.size()) {
      ptr = cx.opts->realloc(ptr, worst_case_size, 1, encoded.size());
      assert(ptr + encoded.size() <= cx.opts->memory.size());
    }
  }
  return std::make_tuple(ptr, encoded.size());
}

std::tuple<uint32_t, uint32_t> store_utf16_to_utf8(const CallContext &cx, const char *src,
                                                   uint32_t src_code_units) {
  uint32_t worst_case_size = src_code_units * 3;
  return store_string_to_utf8(cx, src, src_code_units, worst_case_size);
}

std::tuple<uint32_t, uint32_t> store_latin1_to_utf8(const CallContext &cx, const char *src,
                                                    uint32_t src_code_units) {
  uint32_t worst_case_size = src_code_units * 2;
  return store_string_to_utf8(cx, src, src_code_units, worst_case_size);
}

std::tuple<uint32_t, uint32_t> store_utf8_to_utf16(const CallContext &cx, const char *src,
                                                   uint32_t src_code_units) {
  uint32_t worst_case_size = 2 * src_code_units;
  if (worst_case_size > MAX_STRING_BYTE_LENGTH)
    throw std::runtime_error("Worst case size exceeds maximum string byte length");
  uint32_t ptr = cx.opts->realloc(0, 0, 2, worst_case_size);
  if (ptr != align_to(ptr, 2)) throw std::runtime_error("Pointer misaligned");
  if (ptr + worst_case_size > cx.opts->memory.size())
    throw std::runtime_error("Out of bounds access");
  //  TODO:  std::string encoded = encode(src, "utf-16-le");
  std::string encoded = std::string(src, src_code_units);
  std::memcpy(&cx.opts->memory[ptr], encoded.data(), encoded.size());
  if (encoded.size() < worst_case_size) {
    ptr = cx.opts->realloc(ptr, worst_case_size, 2, encoded.size());
    if (ptr != align_to(ptr, 2)) throw std::runtime_error("Pointer misaligned");
    if (ptr + encoded.size() > cx.opts->memory.size())
      throw std::runtime_error("Out of bounds access");
  }
  uint32_t code_units = static_cast<uint32_t>(encoded.size() / 2);
  return std::make_tuple(ptr, code_units);
}

std::tuple<uint32_t, uint32_t> store_string_to_latin1_or_utf16(const CallContext &cx,
                                                               const char *src,
                                                               uint32_t src_code_units) {
  assert(src_code_units <= MAX_STRING_BYTE_LENGTH);
  uint32_t ptr = cx.opts->realloc(0, 0, 2, src_code_units);
  if (ptr != align_to(ptr, 2)) throw std::runtime_error("Pointer misaligned");
  if (ptr + src_code_units > cx.opts->memory.size())
    throw std::runtime_error("Out of bounds access");
  uint32_t dst_byte_length = 0;
  for (size_t i = 0; i < src_code_units; ++i) {
    char usv = *reinterpret_cast<const char *>(src);
    if (static_cast<uint32_t>(usv) < (1 << 8)) {
      cx.opts->memory[ptr + dst_byte_length] = static_cast<uint32_t>(usv);
      dst_byte_length += 1;
    } else {
      uint32_t worst_case_size = 2 * src_code_units;
      if (worst_case_size > MAX_STRING_BYTE_LENGTH)
        throw std::runtime_error("Worst case size exceeds maximum string byte length");
      ptr = cx.opts->realloc(ptr, src_code_units, 2, worst_case_size);
      if (ptr != align_to(ptr, 2)) throw std::runtime_error("Pointer misaligned");
      if (ptr + worst_case_size > cx.opts->memory.size())
        throw std::runtime_error("Out of bounds access");
      for (int j = dst_byte_length - 1; j >= 0; --j) {
        cx.opts->memory[ptr + 2 * j] = cx.opts->memory[ptr + j];
        cx.opts->memory[ptr + 2 * j + 1] = 0;
      }
      // TODO: Implement encoding to 'utf-16-le'
      std::string encoded = std::string(src, src_code_units);
      std::memcpy(&cx.opts->memory[ptr + 2 * dst_byte_length], encoded.data(), encoded.size());
      if (worst_case_size > encoded.size()) {
        ptr = cx.opts->realloc(ptr, worst_case_size, 2, encoded.size());
        if (ptr != align_to(ptr, 2)) throw std::runtime_error("Pointer misaligned");
        if (ptr + encoded.size() > cx.opts->memory.size())
          throw std::runtime_error("Out of bounds access");
      }
      uint32_t tagged_code_units = static_cast<uint32_t>(encoded.size() / 2) | UTF16_TAG;
      return std::make_tuple(ptr, tagged_code_units);
    }
  }
  if (dst_byte_length < src_code_units) {
    ptr = cx.opts->realloc(ptr, src_code_units, 2, dst_byte_length);
    if (ptr != align_to(ptr, 2)) throw std::runtime_error("Pointer misaligned");
    if (ptr + dst_byte_length > cx.opts->memory.size())
      throw std::runtime_error("Out of bounds access");
  }
  return std::make_tuple(ptr, dst_byte_length);
}

std::tuple<uint32_t, uint32_t> store_probably_utf16_to_latin1_or_utf16(const CallContext &cx,
                                                                       const char *_src,
                                                                       uint32_t src_code_units) {
  uint32_t src_byte_length = 2 * src_code_units;
  if (src_byte_length > MAX_STRING_BYTE_LENGTH)
    throw std::runtime_error("src_byte_length exceeds MAX_STRING_BYTE_LENGTH");

  uint32_t ptr = cx.opts->realloc(0, 0, 2, src_byte_length);
  if (ptr != align_to(ptr, 2)) throw std::runtime_error("ptr is not aligned");

  if (ptr + src_byte_length > cx.opts->memory.size()) throw std::runtime_error("Not enough memory");

  //  TODO:  std::string encoded = encode_utf16le(src);
  std::string src = std::string(_src, src_code_units);
  std::string encoded = src;
  std::copy(encoded.begin(), encoded.end(), cx.opts->memory.begin() + ptr);

  if (std::any_of(src.begin(), src.end(),
                  [](char c) { return static_cast<unsigned char>(c) >= (1 << 8); })) {
    uint32_t tagged_code_units = static_cast<uint32_t>(encoded.size() / 2) | UTF16_TAG;
    return std::make_tuple(ptr, tagged_code_units);
  }

  uint32_t latin1_size = static_cast<uint32_t>(encoded.size() / 2);
  for (uint32_t i = 0; i < latin1_size; ++i)
    cx.opts->memory[ptr + i] = cx.opts->memory[ptr + 2 * i];

  ptr = cx.opts->realloc(ptr, src_byte_length, 1, latin1_size);
  if (ptr + latin1_size > cx.opts->memory.size()) throw std::runtime_error("Not enough memory");

  return std::make_tuple(ptr, latin1_size);
}

std::tuple<uint32_t, uint32_t> store_string_into_range(const CallContext &cx, const Val &v,
                                                       HostEncoding src_encoding
                                                       = HostEncoding::Utf8) {
  const char *src = v.s().ptr;
  const size_t src_tagged_code_units = v.s().len;
  HostEncoding src_simple_encoding;
  uint32_t src_code_units;

  if (src_encoding == HostEncoding::Latin1_Utf16) {
    if (src_tagged_code_units & UTF16_TAG) {
      src_simple_encoding = HostEncoding::Utf16;
      src_code_units = src_tagged_code_units ^ UTF16_TAG;
    } else {
      src_simple_encoding = HostEncoding::Latin1;
      src_code_units = src_tagged_code_units;
    }
  } else {
    src_simple_encoding = src_encoding;
    src_code_units = src_tagged_code_units;
  }

  if (cx.opts->string_encoding == HostEncoding::Utf8) {
    if (src_simple_encoding == HostEncoding::Utf8)
      return store_string_copy(cx, src, src_code_units, 1, 1, "utf-8");
    else if (src_simple_encoding == HostEncoding::Utf16)
      return store_utf16_to_utf8(cx, src, src_code_units);
    else if (src_simple_encoding == HostEncoding::Latin1)
      return store_latin1_to_utf8(cx, src, src_code_units);
  } else if (cx.opts->string_encoding == HostEncoding::Utf16) {
    if (src_simple_encoding == HostEncoding::Utf8)
      return store_utf8_to_utf16(cx, src, src_code_units);
    else if (src_simple_encoding == HostEncoding::Utf16
             || src_simple_encoding == HostEncoding::Latin1)
      return store_string_copy(cx, src, src_code_units, 2, 2, "utf-16-le");
  } else if (cx.opts->string_encoding == HostEncoding::Latin1_Utf16) {
    if (src_encoding == HostEncoding::Utf8 || src_encoding == HostEncoding::Utf16)
      return store_string_to_latin1_or_utf16(cx, src, src_code_units);
    else if (src_encoding == HostEncoding::Latin1_Utf16) {
      if (src_simple_encoding == HostEncoding::Latin1)
        return store_string_copy(cx, src, src_code_units, 1, 2, "latin-1");
      else if (src_simple_encoding == HostEncoding::Utf16)
        return store_probably_utf16_to_latin1_or_utf16(cx, src, src_code_units);
    }
  }

  assert(false);
  return std::make_tuple(0, 0);
}

template <typename T>
void store_int(const CallContext &cx, const T &v, uint32_t ptr, uint8_t nbytes) {
  for (size_t i = 0; i < nbytes; ++i) {
    cx.opts->memory[ptr + i] = static_cast<uint8_t>(v >> (8 * i));
  }
}

void store_string(const CallContext &cx, const Val &v, uint32_t ptr) {
  auto [begin, tagged_code_units] = store_string_into_range(cx, v);
  store_int(cx, begin, ptr, 4);
  store_int(cx, tagged_code_units, ptr + 4, 4);
}

template <typename T> T random_nan_bits(int bits, int quiet_bits) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> distrib(1 << quiet_bits, (1 << bits) - 1);
  return distrib(gen);
}

float32_t core_f32_reinterpret_i32(int32_t i);
float32_t maybe_scramble_nan32(float32_t f) {
  if (std::isnan(f)) {
    if (DETERMINISTIC_PROFILE) {
      f = CANONICAL_FLOAT32_NAN;
    } else {
      f = core_f32_reinterpret_i32(random_nan_bits<int32_t>(32, 8));
    }
    assert(std::isnan(f));
  }
  return f;
}

float64_t core_f64_reinterpret_i64(int64_t i);
float64_t maybe_scramble_nan64(float64_t f) {
  if (std::isnan(f)) {
    if (DETERMINISTIC_PROFILE) {
      f = CANONICAL_FLOAT32_NAN;
    } else {
      f = core_f64_reinterpret_i64(random_nan_bits<int64_t>(64, 11));
    }
    assert(std::isnan(f));
  }
  return f;
}

uint32_t encode_float_as_i32(float32_t f) {
  return std::bit_cast<uint32_t>(maybe_scramble_nan32(f));
}

uint64_t encode_float_as_i64(float64_t f) {
  return std::bit_cast<uint64_t>(maybe_scramble_nan64(f));
}

void store(const CallContext &cx, const Val &v, uint32_t ptr);
std::pair<uint32_t, size_t> store_list_into_range(const CallContext &cx, const std::vector<Val> &v,
                                                  const ValType &elem_type) {
  auto byte_length = v.size() * size(elem_type);
  if (byte_length >= std::numeric_limits<uint32_t>::max()) {
    throw std::runtime_error("byte_length exceeds limit");
  }
  uint32_t ptr = cx.opts->realloc(0, 0, alignment(elem_type), byte_length);
  if (ptr != align_to(ptr, alignment(elem_type))) {
    throw std::runtime_error("ptr not aligned");
  }
  if (ptr + byte_length > cx.opts->memory.size()) {
    throw std::runtime_error("memory overflow");
  }
  for (size_t i = 0; i < v.size(); ++i) {
    store(cx, v[i], ptr + i * size(elem_type));
  }
  return {ptr, v.size()};
}

void store_list(const CallContext &cx, ListPtr list, uint32_t ptr) {
  auto [begin, length] = store_list_into_range(cx, list->vs, list->t);
  store_int(cx, begin, ptr, 4);
  store_int(cx, length, ptr + 4, 4);
}

void store_record(const CallContext &cx, RecordPtr record, uint32_t ptr) {
  for (const auto &f : record->fields) {
    ptr = align_to(ptr, alignment(f.v.value()));
    store(cx, f.v.value(), ptr);
    ptr += size(f.v.value());
  }
}

std::vector<std::string> split(const std::string &input, char delimiter) {
  std::vector<std::string> result;
  std::string token;
  std::istringstream stream(input);

  while (std::getline(stream, token, delimiter)) {
    result.push_back(token);
  }

  return result;
}

std::pair<int, Val> match_case(VariantPtr v) {
  assert(v->cases.size() == 1);
  auto key = v->cases[0].label;
  auto value = v->cases[0].v.value();
  for (auto label : split(key, '|')) {
    auto case_index = find_case(label, v->cases);
    if (case_index != -1) {
      return {case_index, value};
    }
  }
  throw std::runtime_error("Case not found");
}

void store_variant(const CallContext &cx, VariantPtr v, uint32_t ptr) {
  auto [case_index, case_value] = match_case(v);
  auto disc_size = size(discriminant_type(v->cases));
  store_int(cx, case_index, ptr, disc_size);
  ptr += disc_size;
  ptr = align_to(ptr, max_case_alignment(v->cases));
  const auto &c = v->cases[case_index];
  if (c.v.has_value()) {
    store(cx, c.v.value(), ptr);
  }
}

void store(const CallContext &cx, const Val &v, uint32_t ptr) {
  assert(ptr == align_to(ptr, alignment(v)));
  assert(ptr + size(v) <= cx.opts->memory.size());
  switch (v.kind()) {
    case ValType::Bool:
      store_int(cx, v.b(), ptr, 1);
      break;
    case ValType::U8:
      store_int(cx, v.u8(), ptr, 1);
      break;
    case ValType::U16:
      store_int(cx, v.u16(), ptr, 2);
      break;
    case ValType::U32:
      store_int(cx, v.u32(), ptr, 4);
      break;
    case ValType::U64:
      store_int(cx, v.u64(), ptr, 8);
      break;
    case ValType::S8:
      store_int(cx, v.s8(), ptr, 1);
      break;
    case ValType::S16:
      store_int(cx, v.s16(), ptr, 2);
      break;
    case ValType::S32:
      store_int(cx, v.s32(), ptr, 4);
      break;
    case ValType::S64:
      store_int(cx, v.s64(), ptr, 8);
      break;
    case ValType::Float32:
      store_int(cx, encode_float_as_i32(v.f32()), ptr, 4);
      break;
    case ValType::Float64:
      store_int(cx, encode_float_as_i64(v.f64()), ptr, 8);
      break;
    case ValType::Char:
      store_int(cx, char_to_i32(v.c()), ptr, 4);
      break;
    case ValType::String:
      store_string(cx, v, ptr);
      break;
    case ValType::List:
      store_list(cx, v.list(), ptr);
      break;
    case ValType::Record:
      store_record(cx, v.record(), ptr);
      break;
    case ValType::Variant:
      store_variant(cx, v.variant(), ptr);
      break;
    // case ValType::Flags:
    //     store_flags(cx, v.flags(), ptr);
    //     break;
    // case ValType::Own:
    //     store_int(cx, lower_own(cx.opts, v, t), ptr, 4);
    //     break;
    // case ValType::Borrow:
    //     store_int(cx, lower_borrow(cx.opts, v, t), ptr, 4);
    //     break;
    default:
      throw std::runtime_error("Unknown type");
  }
}

//  Flatten  ----------------------------------------------------------------

const int MAX_FLAT_PARAMS = 16;
const int MAX_FLAT_RESULTS = 1;

std::vector<std::string> flatten_type(ValType kind);
std::vector<std::string> flatten_types(const std::vector<ValType> &ts);

// CoreFuncType flatten_functype(FuncTypePtr ft, std::string context)
// {
//     std::vector<std::string> flat_params = flatten_types(ft->param_types());
//     if (flat_params.size() > MAX_FLAT_PARAMS)
//     {
//         flat_params = {"i32"};
//     }

//     std::vector<std::string> flat_results = flatten_types(ft->result_types());
//     if (flat_results.size() > MAX_FLAT_RESULTS)
//     {
//         if (context == "lift")
//         {
//             flat_results = {"i32"};
//         }
//         else if (context == "lower")
//         {
//             flat_params.push_back("i32");
//             flat_results = {};
//         }
//     }

//     return CoreFuncType(flat_params, flat_results);
// }

std::vector<std::string> flatten_types(const std::vector<Val> &vs) {
  std::vector<std::string> result;
  for (Val v : vs) {
    std::vector<std::string> flattened = flatten_type(v.kind());
    result.insert(result.end(), flattened.begin(), flattened.end());
  }
  return result;
}

std::vector<std::string> flatten_types(const std::vector<ValType> &ts) {
  std::vector<std::string> result;
  for (ValType t : ts) {
    std::vector<std::string> flattened = flatten_type(t);
    result.insert(result.end(), flattened.begin(), flattened.end());
  }
  return result;
}

std::vector<std::string> flatten_record(const std::vector<Field> &fields);
std::vector<std::string> flatten_variant(const std::vector<Case> &cases);

std::vector<std::string> flatten_type(ValType kind) {
  switch (kind) {
    case ValType::Bool:
      return {"i32"};
    case ValType::U8:
    case ValType::U16:
    case ValType::U32:
      return {"i32"};
    case ValType::U64:
      return {"i64"};
    case ValType::S8:
    case ValType::S16:
    case ValType::S32:
      return {"i32"};
    case ValType::S64:
      return {"i64"};
    case ValType::Float32:
      return {"f32"};
    case ValType::Float64:
      return {"f64"};
    case ValType::Char:
      return {"i32"};
    case ValType::String:
      return {"i32", "i32"};
    case ValType::List:
      return {"i32", "i32"};
      // case ValType::Record:
      //     return flatten_record(static_cast<const Record &>(t).fields);
      // case ValType::Variant:
      //     return flatten_variant(static_cast<const Variant &>(t).cases);
      // case ValType::Flags:
      //     return std::vector<std::string>(num_i32_flags(static_cast<const Flags &>(t).labels),
      //     "i32");
      // case ValType::Own:
      // case ValType::Borrow:
      //     return {"i32"};
    default:
      throw std::runtime_error("Invalid type");
  }
}

std::vector<std::string> flatten_record(const std::vector<Field> &fields) {
  std::vector<std::string> flat;
  for (const Field &f : fields) {
    auto flattened = flatten_type(f.v.value().kind());
    flat.insert(flat.end(), flattened.begin(), flattened.end());
  }
  return flat;
}

// std::string join(const std::string &a, const std::string &b);
// std::vector<std::string> flatten_variant(const std::vector<Case> &cases)
// {
//     std::vector<std::string> flat;
//     for (const auto &c : cases)
//     {
//         if (c.t.has_value())
//         {
//             auto flattened = flatten_type(c.t.value());
//             for (size_t i = 0; i < flattened.size(); ++i)
//             {
//                 if (i < flat.size())
//                 {
//                     flat[i] = join(flat[i], flattened[i]);
//                 }
//                 else
//                 {
//                     flat.push_back(flattened[i]);
//                 }
//             }
//         }
//     }
//     auto discriminantFlattened = flatten_type(discriminant_type(cases));
//     flat.insert(flat.begin(), discriminantFlattened.begin(), discriminantFlattened.end());
//     return flat;
// }

std::string join(const std::string &a, const std::string &b) {
  if (a == b) return a;
  if ((a == "i32" && b == "f32") || (a == "f32" && b == "i32")) return "i32";
  return "i64";
}

std::vector<WasmVal> lower_flat_string(const CallContext &cx, const Val &v) {
  auto [ptr, packed_length] = store_string_into_range(cx, v);
  return {ptr, packed_length};
}

std::vector<WasmVal> lower_flat(const CallContext &cx, const Val &v) {
  switch (despecialize(v).kind()) {
    case ValType::Bool:
      return {WasmVal(static_cast<uint32_t>(v.b()))};
    case ValType::U8:
      return {WasmVal(static_cast<uint32_t>(v.u8()))};
    case ValType::U16:
      return {WasmVal(static_cast<uint32_t>(v.u16()))};
    case ValType::U32:
      return {WasmVal(static_cast<uint32_t>(v.u32()))};
    case ValType::U64:
      return {WasmVal(static_cast<uint64_t>(v.u64()))};
    case ValType::S8:
      return {WasmVal(static_cast<uint32_t>(v.s8()))};
    case ValType::S16:
      return {WasmVal(static_cast<uint32_t>(v.s16()))};
    case ValType::S32:
      return {WasmVal(static_cast<uint32_t>(v.s32()))};
    case ValType::S64:
      return {WasmVal(static_cast<uint64_t>(v.s64()))};
    case ValType::Float32:
      return {WasmVal(static_cast<float32_t>(maybe_scramble_nan32(v.f32())))};
    case ValType::Float64:
      return {WasmVal(static_cast<float64_t>(maybe_scramble_nan64(v.f64())))};
    case ValType::Char:
      return {WasmVal(char_to_i32(v.c()))};
    case ValType::String:
      return lower_flat_string(cx, v);
    default:
      throw std::runtime_error("Invalid type");
  }
}

// std::vector<Val> lift_values(CallContext &cx, int max_flat, const std::vector<ValType> &ts)
// {
//     auto flat_types = flatten_types(ts);
//     if (flat_types.size() > max_flat)
//     {
//         auto ptr = vi.next('i32');
//         auto tuple_type = Tuple(ts);
//         if (ptr != align_to(ptr, alignment(tuple_type)))
//         {
//             throw std::runtime_error("Misaligned pointer");
//         }
//         if (ptr + size(tuple_type) > cx.opts.memory.size())
//         {
//             throw std::runtime_error("Out of bounds access");
//         }
//         return load(cx, ptr, tuple_type).values();
//     }
//     else
//     {
//         std::vector<Val> result;
//         for (const auto &t : ts)
//         {
//             result.push_back(lift_flat(cx, vi, t));
//         }
//         return result;
//     }
// }

std::vector<WasmVal> lower_values(const CallContext &cx, size_t max_flat,
                                  const std::vector<Val> &vs, int *out_param = nullptr) {
  if (vs.size() > max_flat) {
    TuplePtr tuple = std::make_shared<Tuple>();
    tuple->vs.insert(tuple->vs.end(), vs.begin(), vs.end());
    uint32_t ptr;
    if (out_param == nullptr) {
      ptr = cx.opts->realloc(0, 0, alignment(tuple), size(tuple));
    } else {
      //  TODO:  ptr = out_param.next('i32');
      std::abort();
    }
    if (ptr != align_to(ptr, alignment(ValType::Tuple))
        || ptr + size(ValType::Tuple) > cx.opts->memory.size()) {
      throw std::runtime_error("Out of bounds access");
    }
    store(cx, tuple, ptr);
    return {WasmVal(ptr)};
  } else {
    std::vector<WasmVal> flat_vals;
    for (const Val &v : vs) {
      std::vector<WasmVal> temp = lower_flat(cx, v);
      flat_vals.insert(flat_vals.end(), temp.begin(), temp.end());
    }
    return flat_vals;
  }
}
