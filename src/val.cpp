#include "val.hpp"

#include <cassert>

const char *ValTypeNames[] = {
    "bool",
    "int8_t",
    "uint8_t",
    "int16_t",
    "uint16_t",
    "int32_t",
    "uint32_t",
    "int64_t",
    "uint64_t",
    "float32_t",
    "float64_t",
    "wchar_t",
    "string_ptr",
    "list_ptr",
    "field_ptr",
    "record_ptr",
    "tuple_ptr",
    "case_ptr",
    "variant_ptr",
    "enum_ptr",
    "option_ptr",
    "result_ptr",
    "flags_ptr",
    "unknown1", "unknown2", "unknown3", "unknown4", "unknown5", "unknown6"};

namespace cmcpp
{
    //  Vals  ----------------------------------------------------------------

    ValType valType(const Val &v)
    {
        return std::visit([](auto &&arg) -> ValType
                          {
            using T = std::decay_t<decltype(arg)>;
            return ValTrait<T>::type(); },
                          v);
    }

    const char *valTypeName(ValType type)
    {
        return ValTypeNames[static_cast<uint8_t>(type)];
    }

    bool operator==(const Val &lhs, const Val &rhs)
    {
        return lhs.index() == rhs.index() && std::visit([rhs](auto &&arg1, auto &&arg2) -> bool
                                                        {
            using T = std::decay_t<decltype(arg1)>;
            if constexpr (std::is_same_v<T, char>)
            {
                return arg1 == arg2;
            }
            else if constexpr (std::is_same_v<T, string_ptr>)
            {
                return *arg1 ==(*std::get<string_ptr>(rhs));
            }
            else if constexpr (std::is_same_v<T, list_ptr>)
            {
                return *arg1 ==(*std::get<list_ptr>(rhs));
            }
            else if constexpr (std::is_same_v<T, field_ptr>)
            {
                return *arg1 ==(*std::get<field_ptr>(rhs));
            }
            else if constexpr (std::is_same_v<T, record_ptr>)
            {
                return *arg1 ==(*std::get<record_ptr>(rhs));
            }
            else if constexpr (std::is_same_v<T, tuple_ptr>)
            {
                return *arg1 ==(*std::get<tuple_ptr>(rhs));
            }
            else if constexpr (std::is_same_v<T, case_ptr>)
            {
                return *arg1 ==(*std::get<case_ptr>(rhs));
            }
            else if constexpr (std::is_same_v<T, variant_ptr>)
            {
                return *arg1 ==(*std::get<variant_ptr>(rhs));
            }
            else if constexpr (std::is_same_v<T, enum_ptr>)
            {
                return *arg1 ==(*std::get<enum_ptr>(rhs));
            }
            else if constexpr (std::is_same_v<T, option_ptr>)
            {
                return *arg1 ==(*std::get<option_ptr>(rhs));
            }
            else if constexpr (std::is_same_v<T, result_ptr>)
            {
                return *arg1 ==(*std::get<result_ptr>(rhs));
            }
            else if constexpr (std::is_same_v<T, flags_ptr>)
            {
                return *arg1 ==(*std::get<flags_ptr>(rhs));
            }
            else
            {
                return arg1 == arg2;
            } }, lhs, rhs);
    }

    //  ----------------------------------------------------------------------

    string_t::string_t() {}
    string_t::string_t(const char8_t *ptr, size_t len) : ptr(ptr), len(len) {}
    string_t::string_t(const std::string &_str)
    {
        str = _str;
        ptr = (const char8_t *)str.c_str();
        len = str.size();
    }
    std::string string_t::to_string() const
    {
        return std::string((const char *)ptr, len);
    }

    bool string_t::operator==(const string_t &rhs) const
    {
        if (this == nullptr)
        {
            return true;
        }
        return std::string_view((const char *)ptr, len).compare(std::string_view((const char *)rhs.ptr, rhs.len)) == 0;
    }

    list_t::list_t() {}
    list_t::list_t(const Val &lt) : lt(lt) {}
    list_t::list_t(const Val &lt, const std::vector<Val> &vs) : lt(lt), vs(vs) {}
    bool list_t::operator==(const list_t &rhs) const
    {
        if (lt != rhs.lt)
            return false;

        if (vs.size() != rhs.vs.size())
            return false;

        for (size_t i = 0; i < vs.size(); i++)
        {
            if (vs[i] != rhs.vs[i])
                return false;
        }

        return true;
    }

    field_t::field_t() {}
    field_t::field_t(const std::string &label, const Val &v) : label(label), v(v) {}
    bool field_t::operator==(const field_t &rhs) const
    {
        return label == rhs.label && v == rhs.v;
    }

    record_t::record_t() {}
    record_t::record_t(const std::vector<field_ptr> &fields) : fields(fields) {}
    bool record_t::operator==(const record_t &rhs) const
    {
        if (fields.size() != rhs.fields.size())
            return false;

        for (size_t i = 0; i < fields.size(); i++)
        {
            if (*fields[i] != *rhs.fields[i])
                return false;
        }

        return true;
    }
    Val record_t::find(const std::string &label) const
    {
        for (auto f : fields)
        {
            if (f->label == label)
                return f->v;
        }
        return Val();
    }

    tuple_t::tuple_t() {}
    tuple_t::tuple_t(const std::vector<Val> &vs) : vs(vs) {}
    bool tuple_t::operator==(const tuple_t &rhs) const
    {
        if (vs.size() != rhs.vs.size())
            return false;

        for (size_t i = 0; i < vs.size(); i++)
        {
            if (vs[i] != rhs.vs[i])
                return false;
        }

        return true;
    }

    case_t::case_t() {}
    case_t::case_t(const std::string &label, const std::optional<Val> &v, const std::optional<std::string> &refines) : label(label), v(v), refines(refines) {}
    bool case_t::operator==(const case_t &rhs) const
    {
        return label == rhs.label && v == rhs.v;
    }

    variant_t::variant_t() {}
    variant_t::variant_t(const std::vector<case_ptr> &cases) : cases(cases) {}
    bool variant_t::operator==(const variant_t &rhs) const
    {
        for (auto c : cases)
        {
            for (auto c2 : rhs.cases)
            {
                if (c->label == c2->label)
                {
                    if (!c->v.has_value() && !c2->v.has_value())
                    {
                        return true;
                    }
                    else if (c->v.has_value() && c2->v.has_value() && c->v.value() == c2->v.value())
                    {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    enum_t::enum_t() {}
    enum_t::enum_t(const std::vector<std::string> &labels) : labels(labels) {}
    bool enum_t::operator==(const enum_t &rhs) const
    {
        if (labels.size() != rhs.labels.size())
            return false;

        for (size_t i = 0; i < labels.size(); i++)
        {
            if (labels[i] != rhs.labels[i])
                return false;
        }

        return true;
    }

    option_t::option_t() {}
    option_t::option_t(const Val &v) : v(v) {}
    bool option_t::operator==(const option_t &rhs) const
    {
        return v == rhs.v;
    }

    result_t::result_t() {}
    result_t::result_t(const Val &ok, const Val &error) : ok(ok), error(error) {}
    bool result_t::operator==(const result_t &rhs) const
    {
        return ok == rhs.ok && error == rhs.error;
    }

    flags_t::flags_t() {}
    flags_t::flags_t(const std::vector<std::string> &labels) : labels(labels) {}
    bool flags_t::operator==(const flags_t &rhs) const
    {
        if (labels.size() != rhs.labels.size())
            return false;

        for (size_t i = 0; i < labels.size(); i++)
        {
            if (labels[i] != rhs.labels[i])
                return false;
        }

        return true;
    }

    //  ----------------------------------------------------------------

    ValType wasmValType(const WasmVal &v)
    {
        return std::visit([](auto &&arg) -> ValType
                          {
            using T = std::decay_t<decltype(arg)>;
            return ValTrait<T>::type(); },
                          v);
    }

    const char *ValTypeNames[] = {
        "i32",
        "i64",
        "f32",
        "f64",
        "unknown1", "unknown2", "unknown3", "unknown4", "unknown5", "unknown6"};

    const char *wasmValTypeName(ValType type)
    {
        switch (type)
        {
        case ValType::S32:
            return ValTypeNames[0];
        case ValType::S64:
            return ValTypeNames[1];
        case ValType::F32:
            return ValTypeNames[2];
        case ValType::F64:
            return ValTypeNames[3];
        default:
            assert(false);
            return ValTypeNames[4];
        }
    }
}