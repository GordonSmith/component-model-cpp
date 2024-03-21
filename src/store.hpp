#ifndef STORE_HPP
#define STORE_HPP

#include "context.hpp"
#include "val.hpp"

namespace cmcpp
{

    void store(const CallContext &cx, const Val &v, uint32_t ptr);
    std::pair<uint32_t, uint32_t> store_list_into_range(const CallContext &cx, ListPtr list);
    std::pair<uint32_t, uint32_t> store_string_into_range(const CallContext &cx, StringPtr v, HostEncoding src_encoding = HostEncoding::Utf8);

}
#endif