#include "load.hpp"
#include "util.hpp"

#include <cassert>
#include <iostream>

namespace cmcpp
{

    std::shared_ptr<string_t> load_string_from_range(const CallContext &cx, uint32_t ptr, uint32_t tagged_code_units)
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
        auto [dec_str, dec_len] = decode(&cx.opts->memory[ptr], byte_length, encoding);
        return std::make_shared<string_t>(dec_str, dec_len);
    }
}