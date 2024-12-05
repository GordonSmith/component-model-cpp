#ifndef STORE_HPP
#define STORE_HPP

#include "context.hpp"
#include "val.hpp"

namespace cmcpp
{
    void store(const LiftLowerContext &cx, const Val &v, const Val &t, uint32_t ptr);
    std::pair<uint32_t, uint32_t> store_list_into_range(const LiftLowerContext &cx, const list_ptr &v, const Val &elem_type);
    std::pair<uint32_t, uint32_t> store_string_into_range(const LiftLowerContext &cx, const string_ptr &v, HostEncoding src_encoding = HostEncoding::Utf8);
}
#endif