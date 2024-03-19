#include "load.hpp"
#include "util.hpp"

// #include <any>
#include <cassert>
// #include <cmath>
// #include <cstring>
// #include <map>
// #include <random>
// #include <sstream>

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

using HostStringTuple = std::tuple<const char * /*s*/, std::string /*encoding*/, size_t /*byte length*/>;

HostStringTuple load_string_from_range(const CallContext &cx, uint32_t ptr, uint32_t tagged_code_units)
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

HostStringTuple load_string(const CallContext &cx, uint32_t ptr)
{
  uint32_t begin = load_int<uint32_t>(cx, ptr, 4);
  uint32_t tagged_code_units = load_int<uint32_t>(cx, ptr + 4, 4);
  return load_string_from_range(cx, begin, tagged_code_units);
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

std::map<std::string, bool> load_flags(const CallContext &cx, uint32_t ptr, const std::vector<std::string> &labels)
{
  uint32_t i = load_int<uint32_t>(cx, ptr, size_flags(labels));
  return unpack_flags_from_int(i, labels);
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
