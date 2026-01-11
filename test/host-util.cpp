#include "host-util.hpp"

#include <iostream>
#include <string>
#include <cstring>
#include <cassert>
#include <unicode/ucnv.h>
#include <unicode/unistr.h>

using namespace cmcpp;

void trap(const char *msg)
{
    throw new std::runtime_error(msg);
}

const char *const LATIN1 = "ISO-8859-1";
const char *const UTF8 = "UTF-8";
const char *const UTF16 = "UTF-16-LE";

const char *encodingToICU(const Encoding encoding)
{
    switch (encoding)
    {
    case Encoding::Latin1:
        return LATIN1;
    case Encoding::Utf8:
        return UTF8;
    case Encoding::Utf16:
        return UTF16;
    default:
        return "";
    }
}

std::pair<void *, size_t> convert(void *dest, uint32_t dest_byte_len, const void *src, uint32_t src_byte_len, Encoding from_encoding, Encoding to_encoding)
{
    if (from_encoding == to_encoding)
    {
        assert(dest_byte_len >= src_byte_len);
        if (src_byte_len > 0)
        {
            std::memcpy(dest, src, src_byte_len);
            return std::make_pair(dest, src_byte_len);
        }
        return std::make_pair(nullptr, 0);
    }

    UErrorCode status = U_ZERO_ERROR;
    const char *sourceEncoding = encodingToICU(from_encoding);
    const char *targetEncoding = encodingToICU(to_encoding);

    // Create a converter for the source encoding
    UConverter *sourceConverter = ucnv_open(sourceEncoding, &status);
    if (U_FAILURE(status))
    {
        std::cerr << "Error opening source converter: " << u_errorName(status) << std::endl;
        return std::make_pair(nullptr, 0);
    }

    // Create a converter for the target encoding
    UConverter *targetConverter = ucnv_open(targetEncoding, &status);
    if (U_FAILURE(status))
    {
        std::cerr << "Error opening target converter: " << u_errorName(status) << std::endl;
        ucnv_close(sourceConverter);
        return std::make_pair(nullptr, 0);
    }

    // Convert source string to UnicodeString
    icu::UnicodeString unicodeString((const char *)src, src_byte_len, sourceConverter, status);
    if (U_FAILURE(status))
    {
        std::cerr << "Error converting to UnicodeString: " << u_errorName(status) << std::endl;
        ucnv_close(sourceConverter);
        ucnv_close(targetConverter);
        return std::make_pair(nullptr, 0);
    }

    status = U_ZERO_ERROR;
    auto targetLength = unicodeString.extract((char *)dest, dest_byte_len, targetConverter, status);
    if (U_FAILURE(status))
    {
        std::cerr << "Error extracting target string: " << u_errorName(status) << std::endl;
        ucnv_close(sourceConverter);
        ucnv_close(targetConverter);
        return std::make_pair(nullptr, 0);
    }

    auto retVal = std::make_pair(dest, targetLength);
    ucnv_close(sourceConverter);
    ucnv_close(targetConverter);

    return retVal;
}
