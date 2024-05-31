#ifndef LOAD_HPP
#define LOAD_HPP

#include "context.hpp"
#include "val.hpp"

namespace cmcpp
{
    std::shared_ptr<string_t> load_string_from_range(const CallContext &cx, uint32_t ptr, uint32_t tagged_code_units);
}
#endif