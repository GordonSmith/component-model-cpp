#include "traits.hpp"

namespace cmcpp
{
    WasmValVectorIterator::WasmValVectorIterator(const WasmValVector &v) : it(v.begin()), end(v.end()) {}

    string_t::string_t(Encoding encoding, const char8_t *ptr, size_t byte_len) : encoding(encoding), ptr(ptr), byte_len(byte_len)
    {
    }

    string_t::string_t(const std::string &str) : str_buff(str)
    {
        encoding = Encoding::Utf8;
        ptr = reinterpret_cast<const char8_t *>(str_buff.c_str());
        byte_len = str_buff.size();
    }

    void string_t::operator=(const std::string &str)
    {
        str_buff = str;
        encoding = Encoding::Utf8;
        ptr = reinterpret_cast<const char8_t *>(str_buff.c_str());
        byte_len = str_buff.size();
    }
}
