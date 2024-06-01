#include "val.hpp"

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
    "unknown2", "unknown3", "unknown4", "unknown5", "unknown6"};

namespace cmcpp
{
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

    //  ----------------------------------------------------------------

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
            else
            {
                return arg1 == arg2;
            } }, lhs, rhs);
    }

    const char *valTypeName(ValType type)
    {
        return ValTypeNames[static_cast<uint8_t>(type)];
    }

    ValType valType(const Val &v)
    {
        return std::visit([](auto &&arg) -> ValType
                          {
            using T = std::decay_t<decltype(arg)>;
            return ValTrait<T>::type(); },
                          v);
    }

    // ValType refType(const Ref &v)
    // {
    //     return std::visit([](auto &&arg) -> ValType
    //                       {
    //         using T = std::decay_t<decltype(arg)>;
    //         return ValTrait<T>::type(); },
    //                       v);
    // }

    ValType wasmValType(const WasmVal &v)
    {
        return std::visit([](auto &&arg) -> ValType
                          {
            using T = std::decay_t<decltype(arg)>;
            return ValTrait<T>::type(); },
                          v);
    }

    // ValType wasmRefType(const WasmRef &v)
    // {
    //     return std::visit([](auto &&arg) -> ValType
    //                       {
    //         using T = std::decay_t<decltype(arg)>;
    //         return ValTrait<T>::type(); },
    //                       v);
    // }
}