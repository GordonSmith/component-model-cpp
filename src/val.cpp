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
                auto rhs_ptr = std::get<string_ptr>(rhs);
                if (arg1 == nullptr && rhs_ptr == nullptr)
                {
                    return true;
                }
                if (arg1 == nullptr || rhs_ptr == nullptr)
                {
                    return false;
                }
                return *arg1 == *rhs_ptr;
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

    PrintValVisitor::PrintValVisitor(std::ostream &stream) : stream(stream) {}
    void PrintValVisitor::operator()(const bool &v) const
    {
        stream << (v ? "true" : "false");
    }
    void PrintValVisitor::operator()(const int8_t &v) const
    {
        stream << (int)v;
    }
    void PrintValVisitor::operator()(const uint8_t &v) const
    {
        stream << (unsigned)v;
    }
    void PrintValVisitor::operator()(const int16_t &v) const
    {
        stream << (int)v;
    }
    void PrintValVisitor::operator()(const uint16_t &v) const
    {
        stream << (unsigned)v;
    }
    void PrintValVisitor::operator()(const int32_t &v) const
    {
        stream << v;
    }
    void PrintValVisitor::operator()(const uint32_t &v) const
    {
        stream << v;
    }
    void PrintValVisitor::operator()(const int64_t &v) const
    {
        stream << v;
    }
    void PrintValVisitor::operator()(const uint64_t &v) const
    {
        stream << v;
    }
    void PrintValVisitor::operator()(const float32_t &v) const
    {
        stream << v;
    }
    void PrintValVisitor::operator()(const float64_t &v) const
    {
        stream << v;
    }
    void PrintValVisitor::operator()(const wchar_t &v) const
    {
        stream << "todo"; // v;
    }
    void PrintValVisitor::operator()(const string_ptr &v) const
    {
        if (v == nullptr)
        {
            stream << "null";
        }
        else
        {
            stream << v->to_string();
        }
    }
    void PrintValVisitor::operator()(const list_ptr &v) const
    {
        if (v == nullptr)
        {
            stream << "null";
        }
        else
        {
            stream << "todo"; //*v;
        }
    }
    void PrintValVisitor::operator()(const field_ptr &v) const
    {
        if (v == nullptr)
        {
            stream << "null";
        }
        else
        {
            stream << "todo"; //*v;
        }
    }
    void PrintValVisitor::operator()(const record_ptr &v) const
    {
        if (v == nullptr)
        {
            stream << "null";
        }
        else
        {
            stream << "todo"; //*v;
        }
    }
    void PrintValVisitor::operator()(const tuple_ptr &v) const
    {
        if (v == nullptr)
        {
            stream << "null";
        }
        else
        {
            stream << "todo"; //*v;
        }
    }
    void PrintValVisitor::operator()(const case_ptr &v) const
    {
        if (v == nullptr)
        {
            stream << "null";
        }
        else
        {
            stream << "todo"; //*v;
        }
    }
    void PrintValVisitor::operator()(const variant_ptr &v) const
    {
        if (v == nullptr)
        {
            stream << "null";
        }
        else
        {
            stream << "todo"; //*v;
        }
    }
    void PrintValVisitor::operator()(const enum_ptr &v) const
    {
        if (v == nullptr)
        {
            stream << "null";
        }
        else
        {
            stream << "todo"; //*v;
        }
    }
    void PrintValVisitor::operator()(const option_ptr &v) const
    {
        if (v == nullptr)
        {
            stream << "null";
        }
        else
        {
            stream << "todo"; //*v;
        }
    }
    void PrintValVisitor::operator()(const result_ptr &v) const
    {
        if (v == nullptr)
        {
            stream << "null";
        }
        else
        {
            stream << "todo"; //*v;
        }
    }
    void PrintValVisitor::operator()(const flags_ptr &v) const
    {
        if (v == nullptr)
        {
            stream << "null";
        }
        else
        {
            stream << "todo"; //*v;
        }
    }

    //  ----------------------------------------------------------------------

    string_t::string_t() {}
    string_t::string_t(Encoding encoding, size_t len) : _encoding(encoding)
    {
        owned_string.resize(len);
    }
    string_t::string_t(const char *ptr, Encoding encoding, size_t len) : _encoding(encoding)
    {
        view = std::string_view(ptr, len);
    }
    string_t::string_t(Encoding encoding, const std::string &str) : _encoding(encoding)
    {
        view = std::string_view(str);
    }

    bool string_t::operator==(const string_t &rhs) const
    {
        return view.compare(rhs.to_string()) == 0;
    }

    Encoding string_t::encoding() const
    {
        return _encoding;
    }
    char8_t *string_t::ptr() const
    {
        return (char8_t *)view.data();
    }
    const char *string_t::c_str() const
    {
        return view.data();
    }
    const size_t string_t::byte_len() const
    {
        return view.size();
    }
    void string_t::resize(size_t len)
    {
        view = std::string_view(view.data(), len);
    }
    const std::string_view &string_t::to_string() const
    {
        return view;
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