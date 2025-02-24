#ifndef CMCPP_STRING_HPP
#define CMCPP_STRING_HPP

#include "context.hpp"
#include "integer.hpp"
#include "util.hpp"

namespace cmcpp
{
    namespace string
    {
        const uint32_t MAX_STRING_BYTE_LENGTH = (1U << 31) - 1;

        std::pair<uint32_t, uint32_t> store_string_copy(CallContext &cx, const void *src, uint32_t src_code_units, uint32_t dst_code_unit_size, uint32_t dst_alignment, Encoding dst_encoding);
        std::pair<uint32_t, uint32_t> store_utf16_to_utf8(CallContext &cx, const void *src, uint32_t src_code_units);
        std::pair<uint32_t, uint32_t> store_latin1_to_utf8(CallContext &cx, const void *src, uint32_t src_code_units);
        std::pair<uint32_t, uint32_t> store_utf8_to_utf16(CallContext &cx, const void *src, uint32_t src_code_units);

        template <String T>
        std::pair<uint32_t, uint32_t> store_string_to_latin1_or_utf16(CallContext &cx, const T &v)
        {
            Encoding src_encoding = ValTrait<T>::encoding;
            const auto *src = v.data();
            const size_t src_code_units = v.size();
            const size_t src_byte_length = src_code_units * ValTrait<T>::char_size;

            assert(src_code_units <= MAX_STRING_BYTE_LENGTH);
            uint32_t ptr = cx.realloc(0, 0, 2, src_byte_length);
            trap_if(cx, ptr != align_to(ptr, 2));
            trap_if(cx, ptr + src_code_units > cx.memory.size());
            uint32_t dst_byte_length = 0;
            for (unsigned usv : v)
            {
                // Optimistically assume the character will fit in a single byte (Latin1)
                if (usv < (1 << 8))
                {
                    cx.memory[ptr + dst_byte_length] = static_cast<uint32_t>(usv);
                    dst_byte_length += 1;
                }
                else
                {
                    // If it doesn't, convert it to a UTF-16 sequence
                    uint32_t worst_case_size = 2 * src_code_units;
                    trap_if(cx, worst_case_size > MAX_STRING_BYTE_LENGTH, "Worst case size exceeds maximum string byte length");
                    ptr = cx.realloc(ptr, src_byte_length, 2, worst_case_size);
                    trap_if(cx, ptr != align_to(ptr, 2), "Pointer misaligned");
                    trap_if(cx, ptr + worst_case_size > cx.memory.size(), "Out of bounds access");

#ifdef SIMPLE_UTF16_CONVERSION
                    // Convert entire string to UTF-16 in one go, ignoring the previously computed data  ---
                    auto encoded = cx.convert(&cx.memory[ptr], worst_case_size, src, src_code_units * ValTrait<T>::char_size, src_encoding, Encoding::Utf16);
                    if (encoded.second < worst_case_size)
                    {
                        ptr = cx.realloc(ptr, worst_case_size, 2, encoded.second * 2);
                        trap_if(cx, ptr != align_to(ptr, 2), "Pointer misaligned");
                        trap_if(cx, ptr + encoded.second > cx.memory.size(), "Out of bounds access");
                    }
                    uint32_t tagged_code_units = static_cast<uint32_t>(encoded.second / 2) | UTF16_TAG;
                    return std::make_pair(ptr, tagged_code_units);
#else
                    // Pad out existing non unicode characters  ---
                    for (signed j = dst_byte_length - 1; j >= 0; --j)
                    {
                        cx.memory[ptr + 2 * j] = cx.memory[ptr + j];
                        cx.memory[ptr + 2 * j + 1] = 0;
                    }

                    // Convert the remaining portion  ---
                    uint32_t destPtr = ptr + (2 * dst_byte_length);
                    uint32_t destLen = worst_case_size - (2 * dst_byte_length);
                    void *srcPtr = (char *)src + dst_byte_length * ValTrait<T>::char_size;
                    uint32_t srcLen = (src_code_units - dst_byte_length) * ValTrait<T>::char_size;
                    auto encoded = cx.convert(&cx.memory[destPtr], destLen, srcPtr, srcLen, src_encoding, Encoding::Utf16);

                    // Add special tag to indicate the string is a UTF-16 string  ---
                    uint32_t tagged_code_units = static_cast<uint32_t>(dst_byte_length + encoded.second / 2) | UTF16_TAG;
                    return std::make_pair(ptr, tagged_code_units);
#endif
                }
            }
            if (dst_byte_length < src_code_units)
            {
                ptr = cx.realloc(ptr, src_code_units, 2, dst_byte_length);
                trap_if(cx, ptr != align_to(ptr, 2), "Pointer misaligned");
                trap_if(cx, ptr + dst_byte_length > cx.memory.size(), "Out of bounds access");
            }
            return std::make_pair(ptr, dst_byte_length);
        }

        std::pair<uint32_t, uint32_t> store_probably_utf16_to_latin1_or_utf16(CallContext &cx, const void *src, uint32_t src_code_units);

        template <String T>
        std::pair<offset, bytes> store_into_range(CallContext &cx, const T &v)
        {
            Encoding src_encoding = ValTrait<T>::encoding;
            auto *src = v.data();
            const size_t src_tagged_code_units = v.size();

            Encoding src_simple_encoding;
            uint32_t src_code_units;
            if (src_encoding == Encoding::Latin1_Utf16)
            {
                if (src_tagged_code_units & UTF16_TAG)
                {
                    src_simple_encoding = Encoding::Utf16;
                    src_code_units = src_tagged_code_units ^ UTF16_TAG;
                }
                else
                {
                    src_simple_encoding = Encoding::Latin1;
                    src_code_units = src_tagged_code_units;
                }
            }
            else
            {
                src_simple_encoding = src_encoding;
                src_code_units = src_tagged_code_units;
            }

            switch (cx.guest_encoding)
            {
            case Encoding::Latin1:
                cx.trap("Invalid guest encoding, must be UTF8, UTF16 or Latin1/UTF16");
                break;
            case Encoding::Utf8:
                switch (src_simple_encoding)
                {
                case Encoding::Utf8:
                    return store_string_copy(cx, src, src_code_units, 1, 1, Encoding::Utf8);
                case Encoding::Utf16:
                    return store_utf16_to_utf8(cx, src, src_code_units);
                case Encoding::Latin1:
                    return store_latin1_to_utf8(cx, src, src_code_units);
                }
                break;
            case Encoding::Utf16:
                switch (src_simple_encoding)
                {
                case Encoding::Utf8:
                    return store_utf8_to_utf16(cx, src, src_code_units);
                case Encoding::Utf16:
                    return store_string_copy(cx, src, src_code_units, 2, 2, Encoding::Utf16);
                case Encoding::Latin1:
                    return store_string_copy(cx, src, src_code_units, 2, 2, Encoding::Utf16);
                }
                break;
            case Encoding::Latin1_Utf16:
                switch (src_encoding)
                {
                case Encoding::Utf8:
                    return store_string_to_latin1_or_utf16(cx, v);
                case Encoding::Utf16:
                    return store_string_to_latin1_or_utf16(cx, v);
                case Encoding::Latin1_Utf16:
                    switch (src_simple_encoding)
                    {
                    case Encoding::Latin1:
                        return store_string_copy(cx, src, src_code_units, 1, 2, Encoding::Latin1);
                    case Encoding::Utf16:
                        return store_probably_utf16_to_latin1_or_utf16(cx, src, src_code_units);
                    }
                }
            }
            assert(false);
            return std::make_pair(0, 0);
        }
        void store(CallContext &cx, const string_t &v, uint32_t ptr);

        template <String T>
        WasmValVector lower_flat(CallContext &cx, const T &v)
        {
            auto [ptr, packed_length] = store_into_range(cx, v);
            return {(int32_t)ptr, (int32_t)packed_length};
        }

        template <String T>
        T load_from_range(const CallContext &cx, uint32_t ptr, uint32_t tagged_code_units)
        {
            uint32_t alignment = 0;
            uint32_t byte_length = 0;
            Encoding encoding = Encoding::Utf8;
            switch (cx.guest_encoding)
            {
            case Encoding::Utf8:
                alignment = 1;
                byte_length = tagged_code_units;
                encoding = Encoding::Utf8;
                break;
            case Encoding::Utf16:
                alignment = 2;
                byte_length = 2 * tagged_code_units;
                encoding = Encoding::Utf16;
                break;
            case Encoding::Latin1_Utf16:
                alignment = 2;
                if (tagged_code_units & UTF16_TAG)
                {
                    byte_length = 2 * (tagged_code_units ^ UTF16_TAG);
                    encoding = Encoding::Utf16;
                }
                else
                {
                    byte_length = tagged_code_units;
                    encoding = Encoding::Latin1;
                }
                break;
            default:
                trap_if(cx, false);
            }
            trap_if(cx, ptr != align_to(ptr, alignment));
            trap_if(cx, ptr + byte_length > cx.memory.size());
            size_t char_size = ValTrait<T>::char_size;
            size_t host_byte_length = byte_length * 2;
            T retVal;
            if constexpr (std::is_same<T, latin1_u16string_t>::value)
            {
                retVal.encoding = encoding;
            }
            retVal.resize(host_byte_length);
            auto decoded = cx.convert(retVal.data(), host_byte_length, (void *)&cx.memory[ptr], byte_length, encoding, ValTrait<T>::encoding == Encoding::Latin1_Utf16 ? encoding : ValTrait<T>::encoding);
            if ((decoded.second / char_size) < host_byte_length)
            {
                retVal.resize(decoded.second / char_size);
            }
            return retVal;
        }

        template <String T>
        T load(const CallContext &cx, offset offset)
        {
            auto begin = integer::load<uint32_t>(cx, offset);
            auto tagged_code_units = integer::load<uint32_t>(cx, offset + 4);
            return load_from_range<T>(cx, begin, tagged_code_units);
        }

        template <String T>
        T lift_flat(const CallContext &cx, const CoreValueIter &vi)
        {

            auto ptr = vi.next<int32_t>();
            auto packed_length = vi.next<int32_t>();
            return load_from_range<T>(cx, ptr, packed_length);
        }
    }
}

#endif
