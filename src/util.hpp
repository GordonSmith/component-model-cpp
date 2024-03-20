#ifndef UTIL_HPP
#define UTIL_HPP

#include "context.hpp"
#include "val.hpp"

#include <map>

namespace cmcpp
{
    const uint32_t UTF16_TAG = 1U << 31;
    const bool DETERMINISTIC_PROFILE = false;
    const float32_t CANONICAL_FLOAT32_NAN = 0x7fc00000;
    const float64_t CANONICAL_FLOAT64_NAN = 0x7ff8000000000000;

    uint32_t align_to(uint32_t ptr, uint32_t alignment);
    int alignment(Val _v);
    int alignment(ValType t);
    float32_t canonicalize_nan32(float32_t f);
    float64_t canonicalize_nan64(float64_t f);
    std::string case_label_with_refinements(const Case &c, const std::vector<Case> &cases);
    uint32_t char_to_i32(char c);
    char convert_i32_to_char(int32_t i);
    bool convert_int_to_bool(uint8_t i);

    std::pair<char8_t *, uint32_t> decode(void *src, uint32_t byte_len, GuestEncoding encoding);
    float decode_i32_as_float(int32_t i);
    double decode_i64_as_float(int64_t i);
    ValType despecialize(const ValType t);
    Val despecialize(const Val &v);
    ValType discriminant_type(const std::vector<Case> &cases);

    std::pair<void *, size_t> encode(const char8_t *src, uint32_t byte_len, GuestEncoding encoding);
    uint32_t encode_float_as_i32(float32_t f);
    uint64_t encode_float_as_i64(float64_t f);

    bool isAligned(uint32_t ptr, uint32_t alignment);
    int find_case(const std::string &label, const std::vector<Case> &cases);
    std::pair<int, Val> match_case(VariantPtr v);
    float32_t maybe_scramble_nan32(float32_t f);
    float64_t maybe_scramble_nan64(float64_t f);
    int max_case_alignment(const std::vector<Case> &cases);
    int size(const Val &v);
    int size(ValType t);
    int size_flags(const std::vector<std::string> &labels);
    std::map<std::string, bool> unpack_flags_from_int(int i, const std::vector<std::string> &labels);
}
#endif
