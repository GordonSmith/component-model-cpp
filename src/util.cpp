#include "util.hpp"

#include <cassert>
#include <cmath>
#include <cstring>
#include <random>
#include <sstream>
#include <limits>
#include <codecvt>

namespace cmcpp
{
    size_t encodeTo(void *dest, const char *src, uint32_t byte_len, GuestEncoding encoding)
    {
        switch (encoding)
        {
        case GuestEncoding::Utf8:
        case GuestEncoding::Latin1:
            std::memcpy(dest, src, byte_len);
            return byte_len;
        case GuestEncoding::Utf16le:
            assert(false);
            break;
        default:
            throw std::runtime_error("Invalid encoding");
        }
    }

    CoreValueIter::CoreValueIter(const std::vector<WasmVal> &values) : values(values) {}

    void CoreValueIter::skip()
    {
        ++i;
    }
}
