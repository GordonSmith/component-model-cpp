#ifndef LOAD_HPP
#define LOAD_HPP

#include "context.hpp"
#include "val.hpp"

namespace cmcpp
{
    using HostStringTuple = std::tuple<const char8_t * /*s*/, HostEncoding /*encoding*/, size_t /*byte length*/>;

    ListPtr load_list(const CallContext &cx, uint32_t ptr, ValType t);
    HostStringTuple load_string(const CallContext &cx, uint32_t ptr);
    HostStringTuple load_string_from_range(const CallContext &cx, uint32_t ptr, uint32_t tagged_code_units);
}
#endif