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
    int alignment(const Val &v);
    int alignment(ValType t);

    float32_t canonicalize_nan32(float32_t f);
    float64_t canonicalize_nan64(float64_t f);

    int32_t char_to_i32(wchar_t c);
    wchar_t convert_i32_to_char(const CallContext &cx, int32_t i);
    bool convert_int_to_bool(int32_t i);

    std::pair<char8_t *, uint32_t> decode(void *src, uint32_t byte_len, HostEncoding encoding);
    float32_t decode_i32_as_float(int32_t i);
    float64_t decode_i64_as_float(int64_t i);

    ValType despecialize(const ValType t);
    Val despecialize(const Val &v);
    ValType discriminant_type(const std::vector<case_ptr> &cases);

    // std::u32string encode(const char8_t *src, uint32_t byte_len, GuestEncoding encoding);
    // size_t encodeTo(void *, const char8_t *src, uint32_t byte_len, GuestEncoding encoding);
    uint32_t encode_float_as_i32(float32_t f);
    uint64_t encode_float_as_i64(float64_t f);

    bool isAligned(uint32_t ptr, uint32_t alignment);
    int find_case(const std::string &label, const std::vector<case_ptr> &cases);
    std::pair<int, std::optional<Val>> match_case(const variant_ptr &v, const std::vector<case_ptr> &cases);
    float32_t maybe_scramble_nan32(float32_t f);
    float64_t maybe_scramble_nan64(float64_t f);
    int max_case_alignment(const std::vector<case_ptr> &cases);

    uint8_t elem_size(ValType t);
    uint8_t elem_size(const Val &v);

    int elem_size_flags(const std::vector<std::string> &labels);
    int num_i32_flags(const std::vector<std::string> &labels);

    std::string join(const std::string &a, const std::string &b);

    class CoreValueIter
    {

    private:
        std::size_t _i = 0;

    public:
        const std::vector<WasmVal> values;
        size_t &i;

        CoreValueIter(const std::vector<WasmVal> &values);
        CoreValueIter(const std::vector<WasmVal> &values, std::size_t &i);

        virtual int32_t next(int32_t _) const;
        virtual int64_t next(int64_t _) const;
        virtual float32_t next(float32_t _) const;
        virtual float64_t next(float64_t _) const;

        void skip() const;
    };

}
#endif
