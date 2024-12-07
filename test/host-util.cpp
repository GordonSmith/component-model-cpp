#include "host-util.hpp"
#include <cstring>
#include <cassert>

void trap(const char *msg)
{
    throw new std::runtime_error(msg);
}

std::pair<char8_t *, size_t> encodeTo(void *dest, const char *src, uint32_t byte_len, Encoding from_encoding, PythonEncoding to_encoding)
{
    switch (to_encoding)
    {
    case PythonEncoding::utf_8:
    case PythonEncoding::latin_1:
        std::memcpy(dest, src, byte_len);
        return std::make_pair(reinterpret_cast<char8_t *>(dest), byte_len);
    case PythonEncoding::utf_16_le:
        assert(false);
        break;
    default:
        throw std::runtime_error("Invalid encoding");
    }
}

std::pair<char *, uint32_t> decodeFrom(const void *src, uint32_t byte_len, PythonEncoding from_encoding, Encoding to_encoding)
{
    switch (from_encoding)
    {
    case PythonEncoding::utf_8:
    case PythonEncoding::latin_1:
        return {(char *)src, byte_len};
    case PythonEncoding::utf_16_le:
        assert(false);
        break;
    default:
        throw std::runtime_error("Invalid encoding");
    }
}
