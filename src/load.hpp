#ifndef LOAD_HPP
#define LOAD_HPP

#include "context.hpp"
#include "val.hpp"

namespace cmcpp
{
    ListPtr load_list(const CallContext &cx, uint32_t ptr, ValType t);
    StringPtr load_string(const CallContext &cx, uint32_t ptr);
    StringPtr load_string_from_range(const CallContext &cx, uint32_t ptr, uint32_t tagged_code_units);
}
#endif