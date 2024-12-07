#include "host-util.hpp"
#include <cstring>
#include <cassert>

void trap(const char *msg)
{
    throw new std::runtime_error(msg);
}

std::pair<char8_t *, size_t> convert(char8_t *dest, const char8_t *src, uint32_t byte_len, Encoding from_encoding, Encoding to_encoding)
{
    switch (from_encoding)
    {
    case Encoding::Utf8:
        switch (to_encoding)
        {
        case Encoding::Utf8:
        case Encoding::Latin1:
            std::memcpy(dest, src, byte_len);
            return std::make_pair(reinterpret_cast<char8_t *>(dest), byte_len);
        case Encoding::Utf16:
            assert(false);
            break;
        default:
            throw std::runtime_error("Invalid encoding");
        }
        break;
    }
}
