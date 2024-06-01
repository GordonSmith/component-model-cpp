#include "load.hpp"
#include "util.hpp"

#include <cassert>
#include <iostream>

namespace cmcpp
{

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

    std::shared_ptr<string_t> load_string_from_range(const CallContext &cx, uint32_t ptr, uint32_t tagged_code_units)
    {
        HostEncoding encoding;
        uint32_t byte_length = tagged_code_units;
        uint32_t alignment = 1;
        if (cx.opts->string_encoding == HostEncoding::Utf8)
        {
            alignment = 1;
            byte_length = tagged_code_units;
            encoding = HostEncoding::Utf8;
        }
        else if (cx.opts->string_encoding == HostEncoding::Utf16)
        {
            alignment = 2;
            byte_length = 2 * tagged_code_units;
            encoding = HostEncoding::Latin1_Utf16;
        }
        else if (cx.opts->string_encoding == HostEncoding::Latin1_Utf16)
        {
            alignment = 2;
            if (tagged_code_units & UTF16_TAG)
            {
                byte_length = 2 * (tagged_code_units ^ UTF16_TAG);
                encoding = HostEncoding::Latin1_Utf16;
            }
            else
            {
                byte_length = tagged_code_units;
                encoding = HostEncoding::Latin1;
            }
        }
        assert(isAligned(ptr, alignment));
        assert(ptr + byte_length <= cx.opts->memory.size());
        auto [dec_str, dec_len] = decode(&cx.opts->memory[ptr], byte_length, encoding);
        return std::make_shared<string_t>(dec_str, dec_len);
    }

    std::shared_ptr<string_t> load_string(const CallContext &cx, uint32_t ptr)
    {
        uint32_t begin = load_int<uint32_t>(cx, ptr, 4);
        uint32_t tagged_code_units = load_int<uint32_t>(cx, ptr + 4, 4);
        return load_string_from_range(cx, begin, tagged_code_units);
    }

    std::shared_ptr<list_t> load_list_from_range(const CallContext &cx, uint32_t ptr, uint32_t length, const Val &t)
    {
        assert(ptr == align_to(ptr, alignment(t)));
        assert(ptr + length * elem_size(t) <= cx.opts->memory.size());
        auto list = std::make_shared<list_t>(t);
        for (uint32_t i = 0; i < length; ++i)
        {
            list->vs.push_back(load(cx, ptr + i * elem_size(t), t));
        }
        return list;
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
        case ValType::F32:
            return decode_i32_as_float(load_int<int32_t>(cx, ptr, 4));
        case ValType::F64:
            return decode_i64_as_float(load_int<int64_t>(cx, ptr, 8));
        case ValType::Char:
            return convert_i32_to_char(cx, load_int<int32_t>(cx, ptr, 4));
        case ValType::String:
            return load_string(cx, ptr);
        default:
            throw std::runtime_error("Invalid type");
        }
    }

    Val load(const CallContext &cx, uint32_t ptr, const Val &v)
    {
        switch (valType(v))
        {
            // case ValType::Flags:
            //     return load_flags(cx, ptr, std::get<3>(opt));
            // case ValType::Own:
            //     return lift_own(cx, load_int<uint32_t>(cx, ptr, 4), static_cast<const Own &>(t));
            // case ValType::Borrow:
            //     return lift_borrow(cx, load_int<uint32_t>(cx, ptr, 4), static_cast<const Borrow &>(t));
        default:
            return load(cx, ptr, valType(v));
        }
    }
}