#include "store.hpp"
#include "util.hpp"

#include <cassert>
#include <cstring>
#include <limits>
// #include <sstream>

namespace cmcpp
{
    const uint32_t MAX_STRING_BYTE_LENGTH = (1U << 31) - 1;

    std::pair<uint32_t, uint32_t> store_string_copy(const CallContext &cx, const char8_t *src, uint32_t src_code_units, uint32_t dst_code_unit_size, uint32_t dst_alignment, GuestEncoding dst_encoding)
    {
        uint32_t dst_byte_length = dst_code_unit_size * src_code_units;
        assert(dst_byte_length <= MAX_STRING_BYTE_LENGTH);
        uint32_t ptr = cx.opts->realloc(0, 0, dst_alignment, dst_byte_length);
        assert(ptr == align_to(ptr, dst_alignment));
        assert(ptr + dst_byte_length <= cx.opts->memory.size());
        auto enc_len = encodeTo(&cx.opts->memory[ptr], src, src_code_units, dst_encoding);
        assert(dst_byte_length == enc_len);
        return std::make_pair(ptr, src_code_units);
    }

    std::pair<uint32_t, uint32_t> store_string_to_utf8(const CallContext &cx, const char8_t *src, uint32_t src_code_units, uint32_t worst_case_size)
    {
        assert(worst_case_size <= MAX_STRING_BYTE_LENGTH);
        uint32_t ptr = cx.opts->realloc(0, 0, 1, worst_case_size);
        assert(ptr + src_code_units <= cx.opts->memory.size());
        auto enc_len = encodeTo(&cx.opts->memory[ptr], src, worst_case_size, GuestEncoding::Utf8);
        if (enc_len < worst_case_size)
        {
            assert(enc_len <= MAX_STRING_BYTE_LENGTH);
            ptr = cx.opts->realloc(ptr, src_code_units, 1, enc_len);
        }
        return std::make_pair(ptr, enc_len);
    }

    std::pair<uint32_t, uint32_t> store_utf16_to_utf8(const CallContext &cx, const char8_t *src, uint32_t src_code_units)
    {
        uint32_t worst_case_size = src_code_units * 3;
        return store_string_to_utf8(cx, src, src_code_units, worst_case_size);
    }

    std::pair<uint32_t, uint32_t> store_latin1_to_utf8(const CallContext &cx, const char8_t *src, uint32_t src_code_units)
    {
        uint32_t worst_case_size = src_code_units * 2;
        return store_string_to_utf8(cx, src, src_code_units, worst_case_size);
    }

    std::pair<uint32_t, uint32_t> store_utf8_to_utf16(const CallContext &cx, const char8_t *src, uint32_t src_code_units)
    {
        uint32_t worst_case_size = 2 * src_code_units;
        if (worst_case_size > MAX_STRING_BYTE_LENGTH)
            throw std::runtime_error("Worst case size exceeds maximum string byte length");
        uint32_t ptr = cx.opts->realloc(0, 0, 2, worst_case_size);
        if (ptr != align_to(ptr, 2))
            throw std::runtime_error("Pointer misaligned");
        if (ptr + worst_case_size > cx.opts->memory.size())
            throw std::runtime_error("Out of bounds access");
        auto enc_len = encodeTo(&cx.opts->memory[ptr], src, src_code_units, GuestEncoding::Utf16le);
        if (enc_len < worst_case_size)
        {
            uint32_t cleanup_ptr = ptr;
            ptr = cx.opts->realloc(ptr, worst_case_size, 2, enc_len);
            std::memcpy(&cx.opts->memory[ptr], &cx.opts->memory[ptr], enc_len);
            if (ptr != align_to(ptr, 2))
                throw std::runtime_error("Pointer misaligned");
            if (ptr + enc_len > cx.opts->memory.size())
                throw std::runtime_error("Out of bounds access");
        }
        uint32_t code_units = static_cast<uint32_t>(enc_len / 2);
        return std::make_pair(ptr, code_units);
    }

    std::pair<uint32_t, uint32_t> store_string_to_latin1_or_utf16(const CallContext &cx, const char8_t *src, uint32_t src_code_units)
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
            char8_t usv = *src;
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
                auto enc_len = encodeTo(&cx.opts->memory[ptr + 2 * dst_byte_length], src, src_code_units, GuestEncoding::Utf16le);
                if (worst_case_size > enc_len)
                {
                    //  TODO - skipping the truncation for now...
                    // ptr = cx.opts->realloc(ptr, worst_case_size, 2, enc_len);
                    // if (ptr != align_to(ptr, 2))
                    //     throw std::runtime_error("Pointer misaligned");
                    // if (ptr + enc_len > cx.opts->memory.size())
                    //     throw std::runtime_error("Out of bounds access");
                }
                uint32_t tagged_code_units = static_cast<uint32_t>(enc_len / 2) | UTF16_TAG;
                return std::make_pair(ptr, tagged_code_units);
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
        return std::make_pair(ptr, dst_byte_length);
    }

    std::pair<uint32_t, uint32_t> store_probably_utf16_to_latin1_or_utf16(const CallContext &cx, const char8_t *src, uint32_t src_code_units)
    {
        uint32_t src_byte_length = 2 * src_code_units;
        if (src_byte_length > MAX_STRING_BYTE_LENGTH)
            throw std::runtime_error("src_byte_length exceeds MAX_STRING_BYTE_LENGTH");

        uint32_t ptr = cx.opts->realloc(0, 0, 2, src_byte_length);
        if (ptr != align_to(ptr, 2))
            throw std::runtime_error("ptr is not aligned");

        if (ptr + src_byte_length > cx.opts->memory.size())
            throw std::runtime_error("Not enough memory");

        auto enc_len = encodeTo(&cx.opts->memory[ptr], src, src_code_units, GuestEncoding::Utf16le);
        const uint8_t *enc_src_ptr = &cx.opts->memory[ptr];
        if (std::any_of(enc_src_ptr, enc_src_ptr + enc_len, [](uint8_t c)
                        { return static_cast<unsigned char>(c) >= (1 << 8); }))
        {
            uint32_t tagged_code_units = static_cast<uint32_t>(enc_len / 2) | UTF16_TAG;
            return std::make_pair(ptr, tagged_code_units);
        }

        uint32_t latin1_size = static_cast<uint32_t>(enc_len / 2);
        for (uint32_t i = 0; i < latin1_size; ++i)
            cx.opts->memory[ptr + i] = cx.opts->memory[ptr + 2 * i];

        ptr = cx.opts->realloc(ptr, src_byte_length, 1, latin1_size);
        if (ptr + latin1_size > cx.opts->memory.size())
            throw std::runtime_error("Not enough memory");

        return std::make_pair(ptr, latin1_size);
    }

    std::pair<uint32_t, uint32_t> store_string_into_range(const CallContext &cx, const string_ptr &v, HostEncoding src_encoding)
    {
        const char8_t *src = v->ptr;
        const size_t src_tagged_code_units = v->len;
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
                return store_string_copy(cx, src, src_code_units, 1, 1, GuestEncoding::Utf8);
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
                return store_string_copy(cx, src, src_code_units, 2, 2, GuestEncoding::Utf16le);
        }
        else if (cx.opts->string_encoding == HostEncoding::Latin1_Utf16)
        {
            if (src_encoding == HostEncoding::Utf8 || src_encoding == HostEncoding::Utf16)
                return store_string_to_latin1_or_utf16(cx, src, src_code_units);
            else if (src_encoding == HostEncoding::Latin1_Utf16)
            {
                if (src_simple_encoding == HostEncoding::Latin1)
                    return store_string_copy(cx, src, src_code_units, 1, 2, GuestEncoding::Latin1);
                else if (src_simple_encoding == HostEncoding::Utf16)
                    return store_probably_utf16_to_latin1_or_utf16(cx, src, src_code_units);
            }
        }

        assert(false);
        return std::make_pair(0, 0);
    }

}