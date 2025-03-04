#include "string.hpp"
#include "integer.hpp"
#include "util.hpp"

#include <cassert>
#include <algorithm>

namespace cmcpp
{
    namespace string
    {
        std::pair<uint32_t, uint32_t> store_string_copy(CallContext &cx, const void *src, uint32_t src_code_units, uint32_t dst_code_unit_size, uint32_t dst_alignment, Encoding dst_encoding)
        {
            uint32_t dst_byte_length = dst_code_unit_size * src_code_units;
            trap_if(cx, dst_byte_length > MAX_STRING_BYTE_LENGTH);
            if (dst_byte_length > 0)
            {
                uint32_t ptr = cx.realloc(0, 0, dst_alignment, dst_byte_length);
                trap_if(cx, ptr != align_to(ptr, dst_alignment));
                trap_if(cx, ptr + dst_byte_length > cx.memory.size());
                std::memcpy(&cx.memory[ptr], src, dst_byte_length);
                return std::make_pair(ptr, src_code_units);
            }
            return std::make_pair(0, 0);
        }

        std::pair<uint32_t, uint32_t> store_string_to_utf8(CallContext &cx, Encoding src_encoding, const void *src, uint32_t src_byte_len, uint32_t worst_case_size)
        {
            assert(worst_case_size <= MAX_STRING_BYTE_LENGTH);
            uint32_t ptr = cx.realloc(0, 0, 1, worst_case_size);
            trap_if(cx, ptr + src_byte_len > cx.memory.size());
            auto encoded = cx.convert(&cx.memory[ptr], worst_case_size, src, src_byte_len, src_encoding, Encoding::Utf8);
            if (worst_case_size > encoded.second)
            {
                ptr = cx.realloc(ptr, worst_case_size, 1, encoded.second);
                assert(ptr + encoded.second <= cx.memory.size());
            }
            return std::make_pair(ptr, encoded.second);
        }

        std::pair<uint32_t, uint32_t> store_utf16_to_utf8(CallContext &cx, const void *src, uint32_t src_code_units)
        {
            uint32_t worst_case_size = src_code_units * 3;
            return store_string_to_utf8(cx, Encoding::Utf16, src, src_code_units * 2, worst_case_size);
        }

        std::pair<uint32_t, uint32_t> store_latin1_to_utf8(CallContext &cx, const void *src, uint32_t src_code_units)
        {
            uint32_t worst_case_size = src_code_units * 2;
            return store_string_to_utf8(cx, Encoding::Latin1, src, src_code_units, worst_case_size);
        }

        std::pair<uint32_t, uint32_t> store_utf8_to_utf16(CallContext &cx, const void *src, uint32_t src_code_units)
        {
            uint32_t worst_case_size = 2 * src_code_units;
            trap_if(cx, worst_case_size > MAX_STRING_BYTE_LENGTH);
            uint32_t ptr = cx.realloc(0, 0, 2, worst_case_size);
            trap_if(cx, ptr != align_to(ptr, 2));
            trap_if(cx, ptr + worst_case_size > cx.memory.size());
            auto encoded = cx.convert(&cx.memory[ptr], worst_case_size, src, src_code_units, Encoding::Utf8, Encoding::Utf16);
            if (encoded.second < worst_case_size)
            {
                ptr = cx.realloc(ptr, worst_case_size, 2, encoded.second);
                assert(ptr == align_to(ptr, 2));
                assert(ptr + encoded.second <= cx.memory.size());
            }
            uint32_t code_units = static_cast<uint32_t>(encoded.second / 2);
            return std::make_pair(ptr, code_units);
        }

        std::pair<uint32_t, uint32_t> store_probably_utf16_to_latin1_or_utf16(CallContext &cx, const void *src, uint32_t src_code_units)
        {
            uint32_t src_byte_length = 2 * src_code_units;
            trap_if(cx, src_byte_length > MAX_STRING_BYTE_LENGTH);
            uint32_t ptr = cx.realloc(0, 0, 2, src_byte_length);
            trap_if(cx, ptr != align_to(ptr, 2));
            trap_if(cx, ptr + src_byte_length > cx.memory.size());
            auto encoded = cx.convert(&cx.memory[ptr], src_byte_length, src, src_code_units, Encoding::Utf16, Encoding::Utf16);
            const uint8_t *enc_src_ptr = &cx.memory[ptr];
            if (std::any_of(enc_src_ptr, enc_src_ptr + encoded.second,
                            [](unsigned c)
                            { return c >= (1 << 8); }))
            {
                uint32_t tagged_code_units = static_cast<uint32_t>(encoded.second / 2) | UTF16_TAG;
                return std::make_pair(ptr, tagged_code_units);
            }
            uint32_t latin1_size = static_cast<uint32_t>(encoded.second / 2);
            for (uint32_t i = 0; i < latin1_size; ++i)
                cx.memory[ptr + i] = cx.memory[ptr + 2 * i];
            ptr = cx.realloc(ptr, src_byte_length, 1, latin1_size);
            trap_if(cx, ptr + latin1_size > cx.memory.size());
            return std::make_pair(ptr, latin1_size);
        }

        void store(CallContext &cx, const string_t &v, uint32_t ptr)
        {
            auto [begin, tagged_code_units] = store_into_range(cx, v);
            integer::store(cx, begin, ptr);
            integer::store(cx, tagged_code_units, ptr + 4);
        }

        WasmValVector lower_flat(CallContext &cx, const string_t &v)
        {
            auto [ptr, packed_length] = store_into_range(cx, v);
            return {(int32_t)ptr, (int32_t)packed_length};
        }
    }
}
