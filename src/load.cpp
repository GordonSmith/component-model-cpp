#include "load.hpp"
#include "util.hpp"
#include "lift.hpp"

#include <cassert>
#include <iostream>

namespace cmcpp
{

    template <typename T>
    T load_int(const LiftLowerContext &cx, uint32_t ptr, uint8_t nbytes)
    {
        assert(nbytes == sizeof(T));
        T retVal = 0;
        for (size_t i = 0; i < sizeof(T); ++i)
        {
            retVal |= static_cast<T>(cx.opts->memory[ptr + i]) << (8 * i);
        }
        return retVal;
    }

    string_t load_string_from_range(const LiftLowerContext &cx, uint32_t ptr, uint32_t tagged_code_units)
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
        auto [dec_str, dec_len] = cx.opts->decodeFrom(&cx.opts->memory[ptr], byte_length, encoding);
        return string_t(dec_str, dec_len);
    }

    string_t load_string(const LiftLowerContext &cx, uint32_t ptr)
    {
        uint32_t begin = load_int<uint32_t>(cx, ptr, 4);
        uint32_t tagged_code_units = load_int<uint32_t>(cx, ptr + 4, 4);
        return load_string_from_range(cx, begin, tagged_code_units);
    }

    std::shared_ptr<list_t> load_list_from_range(const LiftLowerContext &cx, uint32_t ptr, uint32_t length, const Val &t)
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

    std::shared_ptr<list_t> load_list(const LiftLowerContext &cx, uint32_t ptr, const Val &t)
    {
        uint32_t begin = load_int<uint32_t>(cx, ptr, 4);
        uint32_t length = load_int<uint32_t>(cx, ptr + 4, 4);
        return load_list_from_range(cx, begin, length, t);
    }

    /*
    def load_record(cx, ptr, fields):
      record = {}
      for field in fields:
        ptr = align_to(ptr, alignment(field.t))
        record[field.label] = load(cx, ptr, field.t)
        ptr += elem_size(field.t)
      return record
    */

    std::shared_ptr<record_t> load_record(const LiftLowerContext &cx, uint32_t ptr, const std::vector<field_ptr> &fields)
    {
        auto record = std::make_shared<record_t>();
        for (auto field : fields)
        {
            ptr = align_to(ptr, alignment(field->v));
            record->fields.push_back(std::make_shared<field_t>(field->label, load(cx, ptr, field->v)));
            ptr += elem_size(field->v);
        }
        return record;
    }

    /*
    def case_label_with_refinements(c, cases):
        label = c.label
        while c.refines is not None:
            c = cases[find_case(c.refines, cases)]
            label += '|' + c.label
        return label
    */

    std::string case_label_with_refinements(case_ptr c, const std::vector<case_ptr> &cases)
    {
        std::string label = c->label;
        while (c->refines.has_value())
        {
            c = cases[find_case(c->refines.value(), cases)];
            label += '|' + c->label;
        }
        return label;
    }

    /*
    def load_variant(cx, ptr, cases):
    disc_size = elem_size(discriminant_type(cases))
    case_index = load_int(cx, ptr, disc_size)
    ptr += disc_size
    trap_if(case_index >= len(cases))
    c = cases[case_index]
    ptr = align_to(ptr, max_case_alignment(cases))
    case_label = case_label_with_refinements(c, cases)
    if c.t is None:
        return { case_label: None }
    return { case_label: load(cx, ptr, c.t) }
    */

    std::shared_ptr<variant_t> load_variant(const LiftLowerContext &cx, uint32_t ptr, const std::vector<case_ptr> &cases)
    {
        uint32_t disc_size = elem_size(discriminant_type(cases));
        uint32_t case_index = load_int<uint32_t>(cx, ptr, disc_size);
        ptr += disc_size;
        assert(case_index < cases.size());
        auto c = cases[case_index];
        ptr = align_to(ptr, max_case_alignment(cases));
        auto case_label = case_label_with_refinements(c, cases);
        if (!c->v.has_value())
        {
            return std::make_shared<variant_t>(std::vector<case_ptr>{std::make_shared<case_t>(case_label)});
        }
        return std::make_shared<variant_t>(std::vector<case_ptr>{std::make_shared<case_t>(case_label, load(cx, ptr, c->v.value()))});
    }

    /*
    def load_flags(cx, ptr, labels):
        i = load_int(cx, ptr, elem_size_flags(labels))
        return unpack_flags_from_int(i, labels)
    */

    flags_ptr load_flags(const LiftLowerContext &cx, uint32_t ptr, const std::vector<std::string> &labels)
    {
        uint32_t i = load_int<uint32_t>(cx, ptr, elem_size_flags(labels));
        return unpack_flags_from_int(i, labels);
    }

    Val load(const LiftLowerContext &cx, uint32_t ptr, ValType t)
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

    Val load(const LiftLowerContext &cx, uint32_t ptr, const Val &v)
    {
        switch (valType(v))
        {
        case ValType::List:
            return load_list(cx, ptr, std::get<list_ptr>(v)->lt);
        case ValType::Record:
            return load_record(cx, ptr, std::get<record_ptr>(v)->fields);
        case ValType::Variant:
            return load_variant(cx, ptr, std::get<variant_ptr>(v)->cases);
        case ValType::Flags:
            return load_flags(cx, ptr, std::get<flags_ptr>(v)->labels);
            // case ValType::Own:
            //     return lift_own(cx, load_int<uint32_t>(cx, ptr, 4), static_cast<const Own &>(t));
            // case ValType::Borrow:
            //     return lift_borrow(cx, load_int<uint32_t>(cx, ptr, 4), static_cast<const Borrow &>(t));
        default:
            return load(cx, ptr, valType(v));
        }
    }
}