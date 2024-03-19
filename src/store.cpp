#include "store.hpp"
#include "util.hpp"

#include <cassert>
#include <cstring>
#include <limits>
// #include <sstream>

namespace cmcpp
{

  std::tuple<uint32_t, uint32_t> store_string_copy(const CallContext &cx, const char *_src, uint32_t src_code_units, uint32_t dst_code_unit_size, uint32_t dst_alignment, const std::string & /* dst_encoding*/)
  {
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

  std::tuple<uint32_t, uint32_t> store_string_to_utf8(const CallContext &cx, const char *src, uint32_t src_code_units, uint32_t worst_case_size)
  {
    assert(src_code_units <= MAX_STRING_BYTE_LENGTH);
    uint32_t ptr = cx.opts->realloc(0, 0, 1, src_code_units);
    assert(ptr + src_code_units <= cx.opts->memory.size());
    //  TODO:  std::string encoded = encode(src, "utf-8");
    std::string encoded = std::string(src, src_code_units);
    assert(src_code_units <= encoded.size());
    std::memcpy(&cx.opts->memory[ptr], encoded.data(), src_code_units);
    if (src_code_units < encoded.size())
    {
      assert(worst_case_size <= MAX_STRING_BYTE_LENGTH);
      ptr = cx.opts->realloc(ptr, src_code_units, 1, worst_case_size);
      assert(ptr + worst_case_size <= cx.opts->memory.size());
      std::memcpy(&cx.opts->memory[ptr + src_code_units], &encoded[src_code_units],
                  encoded.size() - src_code_units);
      if (worst_case_size > encoded.size())
      {
        ptr = cx.opts->realloc(ptr, worst_case_size, 1, encoded.size());
        assert(ptr + encoded.size() <= cx.opts->memory.size());
      }
    }
    return std::make_tuple(ptr, encoded.size());
  }

  std::tuple<uint32_t, uint32_t> store_utf16_to_utf8(const CallContext &cx, const char *src, uint32_t src_code_units)
  {
    uint32_t worst_case_size = src_code_units * 3;
    return store_string_to_utf8(cx, src, src_code_units, worst_case_size);
  }

  std::tuple<uint32_t, uint32_t> store_latin1_to_utf8(const CallContext &cx, const char *src, uint32_t src_code_units)
  {
    uint32_t worst_case_size = src_code_units * 2;
    return store_string_to_utf8(cx, src, src_code_units, worst_case_size);
  }

  std::tuple<uint32_t, uint32_t> store_utf8_to_utf16(const CallContext &cx, const char *src, uint32_t src_code_units)
  {
    uint32_t worst_case_size = 2 * src_code_units;
    if (worst_case_size > MAX_STRING_BYTE_LENGTH)
      throw std::runtime_error("Worst case size exceeds maximum string byte length");
    uint32_t ptr = cx.opts->realloc(0, 0, 2, worst_case_size);
    if (ptr != align_to(ptr, 2))
      throw std::runtime_error("Pointer misaligned");
    if (ptr + worst_case_size > cx.opts->memory.size())
      throw std::runtime_error("Out of bounds access");
    //  TODO:  std::string encoded = encode(src, "utf-16-le");
    std::string encoded = std::string(src, src_code_units);
    std::memcpy(&cx.opts->memory[ptr], encoded.data(), encoded.size());
    if (encoded.size() < worst_case_size)
    {
      ptr = cx.opts->realloc(ptr, worst_case_size, 2, encoded.size());
      if (ptr != align_to(ptr, 2))
        throw std::runtime_error("Pointer misaligned");
      if (ptr + encoded.size() > cx.opts->memory.size())
        throw std::runtime_error("Out of bounds access");
    }
    uint32_t code_units = static_cast<uint32_t>(encoded.size() / 2);
    return std::make_tuple(ptr, code_units);
  }

  std::tuple<uint32_t, uint32_t> store_string_to_latin1_or_utf16(const CallContext &cx, const char *src, uint32_t src_code_units)
  {
    assert(src_code_units <= MAX_STRING_BYTE_LENGTH);
    uint32_t ptr = cx.opts->realloc(0, 0, 2, src_code_units);
    if (ptr != align_to(ptr, 2))
      throw std::runtime_error("Pointer misaligned");
    if (ptr + src_code_units > cx.opts->memory.size())
      throw std::runtime_error("Out of bounds access");
    uint32_t dst_byte_length = 0;
    for (size_t i = 0; i < src_code_units; ++i)
    {
      char usv = *reinterpret_cast<const char *>(src);
      if (static_cast<uint32_t>(usv) < (1 << 8))
      {
        cx.opts->memory[ptr + dst_byte_length] = static_cast<uint32_t>(usv);
        dst_byte_length += 1;
      }
      else
      {
        uint32_t worst_case_size = 2 * src_code_units;
        if (worst_case_size > MAX_STRING_BYTE_LENGTH)
          throw std::runtime_error("Worst case size exceeds maximum string byte length");
        ptr = cx.opts->realloc(ptr, src_code_units, 2, worst_case_size);
        if (ptr != align_to(ptr, 2))
          throw std::runtime_error("Pointer misaligned");
        if (ptr + worst_case_size > cx.opts->memory.size())
          throw std::runtime_error("Out of bounds access");
        for (int j = dst_byte_length - 1; j >= 0; --j)
        {
          cx.opts->memory[ptr + 2 * j] = cx.opts->memory[ptr + j];
          cx.opts->memory[ptr + 2 * j + 1] = 0;
        }
        // TODO: Implement encoding to 'utf-16-le'
        std::string encoded = std::string(src, src_code_units);
        std::memcpy(&cx.opts->memory[ptr + 2 * dst_byte_length], encoded.data(), encoded.size());
        if (worst_case_size > encoded.size())
        {
          ptr = cx.opts->realloc(ptr, worst_case_size, 2, encoded.size());
          if (ptr != align_to(ptr, 2))
            throw std::runtime_error("Pointer misaligned");
          if (ptr + encoded.size() > cx.opts->memory.size())
            throw std::runtime_error("Out of bounds access");
        }
        uint32_t tagged_code_units = static_cast<uint32_t>(encoded.size() / 2) | UTF16_TAG;
        return std::make_tuple(ptr, tagged_code_units);
      }
    }
    if (dst_byte_length < src_code_units)
    {
      ptr = cx.opts->realloc(ptr, src_code_units, 2, dst_byte_length);
      if (ptr != align_to(ptr, 2))
        throw std::runtime_error("Pointer misaligned");
      if (ptr + dst_byte_length > cx.opts->memory.size())
        throw std::runtime_error("Out of bounds access");
    }
    return std::make_tuple(ptr, dst_byte_length);
  }

  std::tuple<uint32_t, uint32_t> store_probably_utf16_to_latin1_or_utf16(const CallContext &cx, const char *_src, uint32_t src_code_units)
  {
    uint32_t src_byte_length = 2 * src_code_units;
    if (src_byte_length > MAX_STRING_BYTE_LENGTH)
      throw std::runtime_error("src_byte_length exceeds MAX_STRING_BYTE_LENGTH");

    uint32_t ptr = cx.opts->realloc(0, 0, 2, src_byte_length);
    if (ptr != align_to(ptr, 2))
      throw std::runtime_error("ptr is not aligned");

    if (ptr + src_byte_length > cx.opts->memory.size())
      throw std::runtime_error("Not enough memory");

    //  TODO:  std::string encoded = encode_utf16le(src);
    std::string src = std::string(_src, src_code_units);
    std::string encoded = src;
    std::copy(encoded.begin(), encoded.end(), cx.opts->memory.begin() + ptr);

    if (std::any_of(src.begin(), src.end(),
                    [](char c)
                    { return static_cast<unsigned char>(c) >= (1 << 8); }))
    {
      uint32_t tagged_code_units = static_cast<uint32_t>(encoded.size() / 2) | UTF16_TAG;
      return std::make_tuple(ptr, tagged_code_units);
    }

    uint32_t latin1_size = static_cast<uint32_t>(encoded.size() / 2);
    for (uint32_t i = 0; i < latin1_size; ++i)
      cx.opts->memory[ptr + i] = cx.opts->memory[ptr + 2 * i];

    ptr = cx.opts->realloc(ptr, src_byte_length, 1, latin1_size);
    if (ptr + latin1_size > cx.opts->memory.size())
      throw std::runtime_error("Not enough memory");

    return std::make_tuple(ptr, latin1_size);
  }

  std::tuple<uint32_t, uint32_t> store_string_into_range(const CallContext &cx, const Val &v, HostEncoding src_encoding)
  {
    const char *src = v.s().ptr;
    const size_t src_tagged_code_units = v.s().len;
    HostEncoding src_simple_encoding;
    uint32_t src_code_units;

    if (src_encoding == HostEncoding::Latin1_Utf16)
    {
      if (src_tagged_code_units & UTF16_TAG)
      {
        src_simple_encoding = HostEncoding::Utf16;
        src_code_units = src_tagged_code_units ^ UTF16_TAG;
      }
      else
      {
        src_simple_encoding = HostEncoding::Latin1;
        src_code_units = src_tagged_code_units;
      }
    }
    else
    {
      src_simple_encoding = src_encoding;
      src_code_units = src_tagged_code_units;
    }

    if (cx.opts->string_encoding == HostEncoding::Utf8)
    {
      if (src_simple_encoding == HostEncoding::Utf8)
        return store_string_copy(cx, src, src_code_units, 1, 1, "utf-8");
      else if (src_simple_encoding == HostEncoding::Utf16)
        return store_utf16_to_utf8(cx, src, src_code_units);
      else if (src_simple_encoding == HostEncoding::Latin1)
        return store_latin1_to_utf8(cx, src, src_code_units);
    }
    else if (cx.opts->string_encoding == HostEncoding::Utf16)
    {
      if (src_simple_encoding == HostEncoding::Utf8)
        return store_utf8_to_utf16(cx, src, src_code_units);
      else if (src_simple_encoding == HostEncoding::Utf16 || src_simple_encoding == HostEncoding::Latin1)
        return store_string_copy(cx, src, src_code_units, 2, 2, "utf-16-le");
    }
    else if (cx.opts->string_encoding == HostEncoding::Latin1_Utf16)
    {
      if (src_encoding == HostEncoding::Utf8 || src_encoding == HostEncoding::Utf16)
        return store_string_to_latin1_or_utf16(cx, src, src_code_units);
      else if (src_encoding == HostEncoding::Latin1_Utf16)
      {
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
  void store_int(const CallContext &cx, const T &v, uint32_t ptr, uint8_t nbytes)
  {
    for (size_t i = 0; i < nbytes; ++i)
    {
      cx.opts->memory[ptr + i] = static_cast<uint8_t>(v >> (8 * i));
    }
  }

  void store_string(const CallContext &cx, const Val &v, uint32_t ptr)
  {
    auto [begin, tagged_code_units] = store_string_into_range(cx, v);
    store_int(cx, begin, ptr, 4);
    store_int(cx, tagged_code_units, ptr + 4, 4);
  }

  std::pair<uint32_t, size_t> store_list_into_range(const CallContext &cx, const std::vector<Val> &v, const ValType &elem_type)
  {
    auto byte_length = v.size() * size(elem_type);
    if (byte_length >= std::numeric_limits<uint32_t>::max())
    {
      throw std::runtime_error("byte_length exceeds limit");
    }
    uint32_t ptr = cx.opts->realloc(0, 0, alignment(elem_type), byte_length);
    if (ptr != align_to(ptr, alignment(elem_type)))
    {
      throw std::runtime_error("ptr not aligned");
    }
    if (ptr + byte_length > cx.opts->memory.size())
    {
      throw std::runtime_error("memory overflow");
    }
    for (size_t i = 0; i < v.size(); ++i)
    {
      store(cx, v[i], ptr + i * size(elem_type));
    }
    return {ptr, v.size()};
  }

  void store_list(const CallContext &cx, ListPtr list, uint32_t ptr)
  {
    auto [begin, length] = store_list_into_range(cx, list->vs, list->t);
    store_int(cx, begin, ptr, 4);
    store_int(cx, length, ptr + 4, 4);
  }

  void store_record(const CallContext &cx, RecordPtr record, uint32_t ptr)
  {
    for (const auto &f : record->fields)
    {
      ptr = align_to(ptr, alignment(f.v.value()));
      store(cx, f.v.value(), ptr);
      ptr += size(f.v.value());
    }
  }

  void store_variant(const CallContext &cx, VariantPtr v, uint32_t ptr)
  {
    auto [case_index, case_value] = match_case(v);
    auto disc_size = size(discriminant_type(v->cases));
    store_int(cx, case_index, ptr, disc_size);
    ptr += disc_size;
    ptr = align_to(ptr, max_case_alignment(v->cases));
    const auto &c = v->cases[case_index];
    if (c.v.has_value())
    {
      store(cx, c.v.value(), ptr);
    }
  }

  void store(const CallContext &cx, const Val &v, uint32_t ptr)
  {
    assert(ptr == align_to(ptr, alignment(v)));
    assert(ptr + size(v) <= cx.opts->memory.size());
    switch (v.kind())
    {
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

}