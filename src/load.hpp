#ifndef LOAD_HPP
#define LOAD_HPP

#include "context.hpp"
#include "val.hpp"

namespace cmcpp
{
    Val load(const CallContext &cx, uint32_t ptr, const ValType t);
    ListPtr load_list(const CallContext &cx, uint32_t ptr, ValType t);
    StringPtr load_string(const CallContext &cx, uint32_t ptr);
    StringPtr load_string_from_range(const CallContext &cx, uint32_t ptr, uint32_t tagged_code_units);
    ListPtr load_list_from_range(const CallContext &cx, uint32_t ptr, uint32_t length, ValType t);
}
#endif