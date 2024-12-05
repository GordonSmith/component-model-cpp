#include "host-util.hpp"
#include <cstring>
#include <cassert>

std::pair<char8_t *, size_t> encodeTo(void *dest, const char *src, uint32_t byte_len, GuestEncoding encoding)
{
    switch (encoding)
    {
    case GuestEncoding::Utf8:
    case GuestEncoding::Latin1:
        std::memcpy(dest, src, byte_len);
        return std::make_pair(reinterpret_cast<char8_t *>(dest), byte_len);
    case GuestEncoding::Utf16le:
        assert(false);
        break;
    default:
        throw std::runtime_error("Invalid encoding");
    }
}

std::pair<char *, uint32_t> decodeFrom(const void *src, uint32_t byte_len, HostEncoding encoding)
{
    switch (encoding)
    {
    case HostEncoding::Utf8:
    case HostEncoding::Latin1:
        return {(char *)src, byte_len};
    case HostEncoding::Latin1_Utf16:
        assert(false);
        break;
    default:
        throw std::runtime_error("Invalid encoding");
    }
}
