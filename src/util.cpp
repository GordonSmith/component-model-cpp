#include "util.hpp"

#include <cassert>
#include <cmath>
#include <cstring>
#include <random>
#include <sstream>
#include <limits>

Val despecialize(const Val &v)
{
  switch (v.kind())
  {
  case ValType::Tuple:
  {
    RecordPtr r = std::make_shared<Record>();
    for (const auto &v2 : v.tuple()->vs)
    {
      Field f("", despecialize(v).kind());
      f.v = v2;
      r->fields.push_back(f);
    }
    return r;
  }
  case ValType::Enum:
  {
    std::vector<Case> cases;
    for (const auto &label : v.enum_()->labels)
    {
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

ValType discriminant_type(const std::vector<Case> &cases)
{
  size_t n = cases.size();

  assert(0 < n && n < std::numeric_limits<unsigned int>::max());
  int match = std::ceil(std::log2(n) / 8);
  switch (match)
  {
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

int alignment_flags(const std::vector<std::string> &labels)
{
  int n = labels.size();
  if (n <= 8)
    return 1;
  if (n <= 16)
    return 2;
  return 4;
}

int alignment(ValType t)
{
  switch (t)
  {
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

int alignment(Val _v)
{
  Val v = despecialize(_v);
  switch (v.kind())
  {
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

int alignment_record(const std::vector<Field> &fields)
{
  int a = 1;
  for (const auto &f : fields)
  {
    a = std::max(a, alignment(f.v.value()));
  }
  return a;
}

int max_case_alignment(const std::vector<Case> &cases)
{
  int a = 1;
  for (const auto &c : cases)
  {
    if (c.v.has_value()) // Check if c.t exists
    {
      a = std::max(a, alignment(c.v.value()));
    }
  }
  return a;
}

int alignment_variant(const std::vector<Case> &cases)
{
  return std::max(alignment(discriminant_type(cases)), max_case_alignment(cases));
}

uint32_t align_to(uint32_t ptr, uint32_t alignment);

int size_record(const std::vector<Field> &fields);
int size_variant(const std::vector<Case> &cases);
int size_flags(const std::vector<std::string> &labels);

int size(ValType t)
{
  switch (t)
  {
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

int size(const Val &v)
{
  ValType kind = despecialize(v).kind();
  switch (kind)
  {
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

int size_record(const std::vector<Field> &fields)
{
  int s = 0;
  for (const auto &f : fields)
  {
    s = align_to(s, alignment(f.v.value()));
    s += size(f.v.value());
  }
  assert(s > 0);
  return align_to(s, alignment_record(fields));
}

uint32_t align_to(uint32_t ptr, uint32_t alignment)
{
  return (ptr + alignment - 1) & ~(alignment - 1);
}

int size_variant(const std::vector<Case> &cases)
{
  int s = size(discriminant_type(cases));
  s = align_to(s, max_case_alignment(cases));
  int cs = 0;
  for (const auto &c : cases)
  {
    if (c.v.has_value())
    {
      cs = std::max(cs, size(c.v.value()));
    }
  }
  s += cs;
  return align_to(s, alignment_variant(cases));
}

int num_i32_flags(const std::vector<std::string> &labels);

int size_flags(const std::vector<std::string> &labels)
{
  int n = labels.size();
  assert(n > 0);
  if (n <= 8)
    return 1;
  if (n <= 16)
    return 2;
  return 4 * num_i32_flags(labels);
}

int num_i32_flags(const std::vector<std::string> &labels)
{
  return std::ceil(static_cast<double>(labels.size()) / 32);
}

bool isAligned(uint32_t ptr, uint32_t alignment) { return (ptr & (alignment - 1)) == 0; }

//  loading
//  --------------------------------------------------------------------------------------------

Val load(const CallContext &cx, uint32_t ptr, ValType t);
Val load(const CallContext &cx, uint32_t ptr, ValType t, ValType lt);
Val load(const CallContext &cx, uint32_t ptr, ValType t, const std::vector<Field> &fields);
Val load(const CallContext &cx, uint32_t ptr, ValType t, const std::vector<Case> &cases);

template <typename T>
T load_int(const CallContext &cx, uint32_t ptr, uint8_t)
{
  T retVal = 0;
  for (size_t i = 0; i < sizeof(T); ++i)
  {
    retVal |= static_cast<T>(cx.opts->memory[ptr + i]) << (8 * i);
  }
  return retVal;
}

bool convert_int_to_bool(uint8_t i) { return i > 0; }

float canonicalize_nan32(float32_t f)
{
  if (std::isnan(f))
  {
    f = CANONICAL_FLOAT32_NAN;
    assert(std::isnan(f));
  }
  return f;
}

float64_t canonicalize_nan64(float64_t f)
{
  if (std::isnan(f))
  {
    f = CANONICAL_FLOAT64_NAN;
    assert(std::isnan(f));
  }
  return f;
}

float core_f32_reinterpret_i32(int32_t i);
float decode_i32_as_float(int32_t i) { return canonicalize_nan32(core_f32_reinterpret_i32(i)); }

double core_f64_reinterpret_i64(int64_t i);
double decode_i64_as_float(int64_t i) { return canonicalize_nan64(core_f64_reinterpret_i64(i)); }

float32_t core_f32_reinterpret_i32(int32_t i)
{
  float f;
  std::memcpy(&f, &i, sizeof f);
  return f;
}

float64_t core_f64_reinterpret_i64(int64_t i)
{
  double d;
  std::memcpy(&d, &i, sizeof d);
  return d;
}

char convert_i32_to_char(int32_t i)
{
  assert(i < 0x110000);
  assert(!(0xD800 <= i && i <= 0xDFFF));
  return static_cast<char>(i);
}

using HostStringTuple = std::tuple<const char * /*s*/, std::string /*encoding*/, size_t /*byte length*/>;

HostStringTuple load_string_from_range(const CallContext &cx, uint32_t ptr,
                                       uint32_t tagged_code_units);
HostStringTuple load_string(const CallContext &cx, uint32_t ptr)
{
  uint32_t begin = load_int<uint32_t>(cx, ptr, 4);
  uint32_t tagged_code_units = load_int<uint32_t>(cx, ptr + 4, 4);
  return load_string_from_range(cx, begin, tagged_code_units);
}

HostStringTuple load_string_from_range(const CallContext &cx, uint32_t ptr,
                                       uint32_t tagged_code_units)
{
  std::string encoding = "utf-8";
  uint32_t byte_length = tagged_code_units;
  uint32_t alignment = 1;
  if (cx.opts->string_encoding == HostEncoding::Utf8)
  {
    alignment = 1;
    byte_length = tagged_code_units;
    encoding = "utf-8";
  }
  else if (cx.opts->string_encoding == HostEncoding::Utf16)
  {
    alignment = 2;
    byte_length = 2 * tagged_code_units;
    encoding = "utf-16-le";
  }
  else if (cx.opts->string_encoding == HostEncoding::Latin1_Utf16)
  {
    alignment = 2;
    if (tagged_code_units & UTF16_TAG)
    {
      byte_length = 2 * (tagged_code_units ^ UTF16_TAG);
      encoding = "utf-16-le";
    }
    else
    {
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

ListPtr load_list(const CallContext &cx, uint32_t ptr, ValType t)
{
  uint32_t begin = load_int<uint32_t>(cx, ptr, 4);
  uint32_t length = load_int<uint32_t>(cx, ptr + 4, 4);
  return load_list_from_range(cx, begin, length, t);
}

ListPtr load_list_from_range(const CallContext &cx, uint32_t ptr, uint32_t length, ValType t)
{
  assert(ptr == align_to(ptr, alignment(t)));
  assert(ptr + length * size(t) <= cx.opts->memory.size());
  auto list = std::make_shared<List>(t);
  for (uint32_t i = 0; i < length; ++i)
  {
    list->vs.push_back(load(cx, ptr + i * size(t), t));
  }
  return list;
}

RecordPtr load_record(const CallContext &cx, uint32_t ptr, const std::vector<Field> &fields)
{
  RecordPtr retVal = std::make_shared<Record>();
  for (auto &field : fields)
  {
    ptr = align_to(ptr, alignment(field.t));
    Field f(field.label, field.t);
    f.v = load(cx, ptr, field.t);
    retVal->fields.push_back(f);
    ptr += size(f.t);
  }
  return retVal;
}

VariantPtr load_variant(const CallContext &cx, uint32_t ptr, const std::vector<Case> &cases)
{
  uint32_t disc_size = size(discriminant_type(cases));
  uint32_t case_index = load_int<uint32_t>(cx, ptr, disc_size);
  ptr += disc_size;
  if (case_index >= cases.size())
  {
    throw std::runtime_error("case_index out of range");
  }
  const Case &c = cases[case_index];
  ptr = align_to(ptr, max_case_alignment(cases));
  std::string case_label = case_label_with_refinements(c, cases);
  if (!c.v.has_value())
  {
    Case c2(case_label, nullptr, c.refines);
    std::vector<Case> cases2 = {c2};
    return std::make_shared<Variant>(cases2);
  }
  else
  {
    Case c2(case_label, load(cx, ptr, c.v.value().kind()), c.refines);
    std::vector<Case> cases2 = {c2};
    return std::make_shared<Variant>(cases2);
  }
}

std::string case_label_with_refinements(const Case &c, const std::vector<Case> &)
{
  std::string label = c.label;
  Case currentCase = c;

  while (currentCase.refines.has_value())
  {
    // TODO:  currentCase = cases[find_case(currentCase.refines.value(), cases)];
    label += '|' + currentCase.label;
  }

  return label;
}

int find_case(const std::string &label, const std::vector<Case> &cases)
{
  auto it = std::find_if(cases.begin(), cases.end(),
                         [&label](const Case &c)
                         { return c.label == label; });

  if (it != cases.end())
  {
    return std::distance(cases.begin(), it);
  }

  return -1;
}

std::map<std::string, bool> load_flags(const CallContext &cx, uint32_t ptr, const std::vector<std::string> &labels)
{
  uint32_t i = load_int<uint32_t>(cx, ptr, size_flags(labels));
  return unpack_flags_from_int(i, labels);
}

std::map<std::string, bool> unpack_flags_from_int(int i, const std::vector<std::string> &labels)
{
  std::map<std::string, bool> record;
  for (const auto &l : labels)
  {
    record[l] = bool(i & 1);
    i >>= 1;
  }
  return record;
}

Val load(const CallContext &cx, uint32_t ptr, ValType t)
{
  switch (t)
  {
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
  case ValType::String:
  {
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
Val load(const CallContext &cx, uint32_t ptr, ValType t, ValType lt)
{
  switch (t)
  {
  case ValType::List:
    return load_list(cx, ptr, lt);
  default:
    throw std::runtime_error("Invalid type");
  }
}
Val load(const CallContext &cx, uint32_t ptr, ValType t, const std::vector<Field> &fields)
{
  switch (t)
  {
  case ValType::Record:
    return load_record(cx, ptr, fields);
  default:
    throw std::runtime_error("Invalid type");
  }
}
Val load(const CallContext &cx, uint32_t ptr, ValType t, const std::vector<Case> &cases)
{
  switch (t)
  {
  case ValType::Variant:
    return load_variant(cx, ptr, cases);
  default:
    throw std::runtime_error("Invalid type");
  }
}
//  Storing ----------------------------------------------------------------

uint32_t char_to_i32(char c) { return static_cast<uint32_t>(c); }

template <typename T>
T random_nan_bits(int bits, int quiet_bits)
{
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> distrib(1 << quiet_bits, (1 << bits) - 1);
  return distrib(gen);
}

float32_t core_f32_reinterpret_i32(int32_t i);
float32_t maybe_scramble_nan32(float32_t f)
{
  if (std::isnan(f))
  {
    if (DETERMINISTIC_PROFILE)
    {
      f = CANONICAL_FLOAT32_NAN;
    }
    else
    {
      f = core_f32_reinterpret_i32(random_nan_bits<int32_t>(32, 8));
    }
    assert(std::isnan(f));
  }
  return f;
}

float64_t core_f64_reinterpret_i64(int64_t i);
float64_t maybe_scramble_nan64(float64_t f)
{
  if (std::isnan(f))
  {
    if (DETERMINISTIC_PROFILE)
    {
      f = CANONICAL_FLOAT32_NAN;
    }
    else
    {
      f = core_f64_reinterpret_i64(random_nan_bits<int64_t>(64, 11));
    }
    assert(std::isnan(f));
  }
  return f;
}

uint32_t encode_float_as_i32(float32_t f)
{
  return std::bit_cast<uint32_t>(maybe_scramble_nan32(f));
}

uint64_t encode_float_as_i64(float64_t f)
{
  return std::bit_cast<uint64_t>(maybe_scramble_nan64(f));
}

std::vector<std::string> split(const std::string &input, char delimiter)
{
  std::vector<std::string> result;
  std::string token;
  std::istringstream stream(input);

  while (std::getline(stream, token, delimiter))
  {
    result.push_back(token);
  }

  return result;
}

std::pair<int, Val> match_case(VariantPtr v)
{
  assert(v->cases.size() == 1);
  auto key = v->cases[0].label;
  auto value = v->cases[0].v.value();
  for (auto label : split(key, '|'))
  {
    auto case_index = find_case(label, v->cases);
    if (case_index != -1)
    {
      return {case_index, value};
    }
  }
  throw std::runtime_error("Case not found");
}

std::string join(const std::string &a, const std::string &b)
{
  if (a == b)
    return a;
  if ((a == "i32" && b == "f32") || (a == "f32" && b == "i32"))
    return "i32";
  return "i64";
}
