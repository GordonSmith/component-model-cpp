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
    float32_t canonicalize_nan32(float32_t f);
    float64_t canonicalize_nan64(float64_t f);

    int32_t char_to_i32(wchar_t c);
    wchar_t convert_i32_to_char(const CallContext &cx, int32_t i);
    bool convert_int_to_bool(int32_t i);

    std::pair<char8_t *, uint32_t> decode(void *src, uint32_t byte_len, HostEncoding encoding);
    ValType despecialize(const ValType t);
    Val despecialize(const Val &v);

    size_t encodeTo(void *, const char8_t *src, uint32_t byte_len, GuestEncoding encoding);

    bool isAligned(uint32_t ptr, uint32_t alignment);
    float32_t maybe_scramble_nan32(float32_t f);
    float64_t maybe_scramble_nan64(float64_t f);

    class CoreValueIter
    {

    public:
        std::vector<WasmVal> values;
        mutable size_t i = 0;

        CoreValueIter(const std::vector<WasmVal> &values);

        template <typename T>
        T next() const
        {
            return std::get<T>(values[i++]);
        }

        void skip();
    };

}
#endif
