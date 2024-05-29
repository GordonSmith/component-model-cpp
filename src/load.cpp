#include "load.hpp"
#include "util.hpp"

#include <cassert>
#include <iostream>

namespace cmcpp
{

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

    String load_string_from_range(const CallContext &cx, uint32_t ptr, uint32_t tagged_code_units)
    {
        GuestEncoding encoding;
        uint32_t byte_length = tagged_code_units;
        uint32_t alignment = 1;
        if (cx.opts->string_encoding == HostEncoding::Utf8)
        {
            alignment = 1;
            byte_length = tagged_code_units;
            encoding = GuestEncoding::Utf8;
        }
        else if (cx.opts->string_encoding == HostEncoding::Utf16)
        {
            alignment = 2;
            byte_length = 2 * tagged_code_units;
            encoding = GuestEncoding::Utf16le;
        }
        else if (cx.opts->string_encoding == HostEncoding::Latin1_Utf16)
        {
            alignment = 2;
            if (tagged_code_units & UTF16_TAG)
            {
                byte_length = 2 * (tagged_code_units ^ UTF16_TAG);
                encoding = GuestEncoding::Utf16le;
            }
            else
            {
                byte_length = tagged_code_units;
                encoding = GuestEncoding::Latin1;
            }
        }
        assert(isAligned(ptr, alignment));
        assert(ptr + byte_length <= cx.opts->memory.size());
        auto [dec_str, dec_len] = decode(&cx.opts->memory[ptr], byte_length, encoding);
        return {dec_str, dec_len};
    }

    String load_string(const CallContext &cx, uint32_t ptr)
    {
        uint32_t begin = load_int<uint32_t>(cx, ptr, 4);
        uint32_t tagged_code_units = load_int<uint32_t>(cx, ptr + 4, 4);
        return load_string_from_range(cx, begin, tagged_code_units);
    }

    List load_list_from_range(const CallContext &cx, uint32_t ptr, uint32_t length, ValType t)
    {
        assert(ptr == align_to(ptr, alignment(t)));
        assert(ptr + length * elem_size(t) <= cx.opts->memory.size());
        List list;
        list.lt = t;
        for (uint32_t i = 0; i < length; ++i)
        {
            list.vs.push_back(load(cx, ptr + i * elem_size(t), t));
        }
        return list;
    }

    List load_list(const CallContext &cx, uint32_t ptr, ValType t)
    {
        uint32_t begin = load_int<uint32_t>(cx, ptr, 4);
        uint32_t length = load_int<uint32_t>(cx, ptr + 4, 4);
        return load_list_from_range(cx, begin, length, t);
    }

    Record load_record(const CallContext &cx, uint32_t ptr, const std::vector<Field> &fields)
    {
        Record retVal;
        for (auto field : fields)
        {
            ptr = align_to(ptr, alignment(field.t));
            Field f = {field.label, load(cx, ptr, field.t)};
            retVal.fields.push_back(f);
            ptr += elem_size(f.t);
        }
        return retVal;
    }

    Variant load_variant(const CallContext &cx, uint32_t ptr, const std::vector<Case> &cases)
    {
        uint32_t disc_size = elem_size(discriminant_type(cases));
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
            return Variant({{case_label, std::nullopt, c.refines}});
        }
        else
        {
            return Variant({{case_label, load(cx, ptr, type(c.v.value())), c.refines}});
        }
    }

    std::map<std::string, bool> load_flags(const CallContext &cx, uint32_t ptr, const std::vector<std::string> &labels)
    {
        uint32_t i = load_int<uint32_t>(cx, ptr, elem_size_flags(labels));
        return unpack_flags_from_int(i, labels);
    }

    Val load(const CallContext &cx, uint32_t ptr, ValType t)
    {
        if (t == typeid(Bool))
            return Bool(convert_int_to_bool(load_int<uint8_t>(cx, ptr, 1)));
        else if (t == typeid(U8))
            return U8(load_int<uint8_t>(cx, ptr, 1));
        else if (t == typeid(U16))
            return U16(load_int<uint16_t>(cx, ptr, 2));
        else if (t == typeid(U32))
            return U32(load_int<uint32_t>(cx, ptr, 4));
        else if (t == typeid(U64))
            return U64(load_int<uint64_t>(cx, ptr, 8));
        else if (t == typeid(S8))
            return S8(load_int<int8_t>(cx, ptr, 1));
        else if (t == typeid(S16))
            return S16(load_int<int16_t>(cx, ptr, 2));
        else if (t == typeid(S32))
            return S32(load_int<int32_t>(cx, ptr, 4));
        else if (t == typeid(S64))
            return S64(load_int<int64_t>(cx, ptr, 8));
        else if (t == typeid(F32))
            return F32(decode_i32_as_float(load_int<int32_t>(cx, ptr, 4)));
        else if (t == typeid(F64))
            return F64(decode_i64_as_float(load_int<int64_t>(cx, ptr, 8)));
        else if (t == typeid(Char))
            return Char(convert_i32_to_char(cx, load_int<int32_t>(cx, ptr, 4)));
        else if (t == typeid(String))
            return load_string(cx, ptr);
        // case ValType::Flags:
        //     return load_flags(cx, ptr, std::get<3>(opt));
        // case ValType::Own:
        //     return lift_own(cx, load_int<uint32_t>(cx, ptr, 4), static_cast<const Own &>(t));
        // case ValType::Borrow:
        //     return lift_borrow(cx, load_int<uint32_t>(cx, ptr, 4), static_cast<const Borrow &>(t));
        else
            throw std::runtime_error("Invalid type");
    }

    Val load(const CallContext &cx, uint32_t ptr, Val v)
    {
        // switch (type(v))
        // {
        // case ValType::Flags:
        //     return load_flags(cx, ptr, std::get<3>(opt));
        // case ValType::Own:
        //     return lift_own(cx, load_int<uint32_t>(cx, ptr, 4), static_cast<const Own &>(t));
        // case ValType::Borrow:
        //     return lift_borrow(cx, load_int<uint32_t>(cx, ptr, 4), static_cast<const Borrow &>(t));
        // default:
        return load(cx, ptr, type(v));
        // }
    }

    Val load(const CallContext &cx, uint32_t ptr, ValType t, ValType lt)
    {
        if (t == typeid(List))
            return load_list(cx, ptr, lt);
        else
            throw std::runtime_error("Invalid type");
    }

    Val load(const CallContext &cx, uint32_t ptr, ValType t, const std::vector<Field> &fields)
    {
        if (t == typeid(Record))
            return load_record(cx, ptr, fields);
        else
            throw std::runtime_error("Invalid type");
    }

    Val load(const CallContext &cx, uint32_t ptr, ValType t, const std::vector<Case> &cases)
    {
        if (t == typeid(Variant))
            return load_variant(cx, ptr, cases);
        else
            throw std::runtime_error("Invalid type");
    }
}