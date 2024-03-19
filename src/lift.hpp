#ifndef LIFT_HPP
#define LIFT_HPP

#include "context.hpp"
#include "val.hpp"

namespace cmcpp
{
    std::vector<Val> lift_values(const CallContext &cx, const std::vector<WasmVal> &vs, const std::vector<ValType> &ts, int max_flat = 16);
}

#endif