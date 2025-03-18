#ifndef CMCPP_STRING_HPP
#define CMCPP_STRING_HPP

#include <cmcpp/context.hpp>
#include <cmcpp/integer.hpp>
#include <cmcpp/util.hpp>

namespace cmcpp
{
    namespace string
    {
        const uint32_t MAX_STRING_BYTE_LENGTH = (1U << 31) - 1;

        inline std::pair<uint32_t, uint32_t> store_string_copy(CallContext &cx, const void *src, uint32_t src_code_units, uint32_t dst_code_unit_size, uint32_t dst_alignment, Encoding dst_encoding)
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

        inline std::pair<uint32_t, uint32_t> store_string_to_utf8(CallContext &cx, Encoding src_encoding, const void *src, uint32_t src_byte_len, uint32_t worst_case_size)
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

        inline std::pair<uint32_t, uint32_t> store_utf16_to_utf8(CallContext &cx, const void *src, uint32_t src_code_units)
        {
            uint32_t worst_case_size = src_code_units * 3;
            return store_string_to_utf8(cx, Encoding::Utf16, src, src_code_units * 2, worst_case_size);
        }

        inline std::pair<uint32_t, uint32_t> store_latin1_to_utf8(CallContext &cx, const void *src, uint32_t src_code_units)
        {
            uint32_t worst_case_size = src_code_units * 2;
            return store_string_to_utf8(cx, Encoding::Latin1, src, src_code_units, worst_case_size);
        }

        inline std::pair<uint32_t, uint32_t> store_utf8_to_utf16(CallContext &cx, const void *src, uint32_t src_code_units)
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

        inline std::pair<uint32_t, uint32_t> store_probably_utf16_to_latin1_or_utf16(CallContext &cx, const void *src, uint32_t src_code_units)
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

        template <String T>
        inline void store(CallContext &cx, const T &v, uint32_t ptr)
        {
            auto [begin, tagged_code_units] = store_into_range(cx, v);
            integer::store(cx, begin, ptr);
            integer::store(cx, tagged_code_units, ptr + 4);
        }

        template <String T>
        inline WasmValVector lower_flat(CallContext &cx, const T &v)
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
            if (vi.done())
            {
                return cmcpp::string::load<T>(cx, ptr);
            }
            auto packed_length = vi.next<int32_t>();
            return load_from_range<T>(cx, ptr, packed_length);
        }
    }

    template <String T>
    inline void store(CallContext &cx, const T &v, uint32_t ptr)
    {
        string::store(cx, v, ptr);
    }

    template <String T>
    inline WasmValVector lower_flat(CallContext &cx, const T &v)
    {
        return string::lower_flat<T>(cx, v);
    }

    template <String T>
    inline T load(const CallContext &cx, uint32_t ptr)
    {
        return string::load<T>(cx, ptr);
    }

    template <String T>
    inline T lift_flat(const CallContext &cx, const CoreValueIter &vi)
    {
        return string::lift_flat<T>(cx, vi);
    }
}

#endif
