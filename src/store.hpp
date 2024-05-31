#ifndef STORE_HPP
#define STORE_HPP

#include "context.hpp"
#include "val.hpp"

namespace cmcpp
{
    std::pair<uint32_t, uint32_t> store_string_into_range(const CallContext &cx, const string_ptr &v, HostEncoding src_encoding = HostEncoding::Utf8);
}
#endif