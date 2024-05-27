#ifndef LIFT_HPP
#define LIFT_HPP

#include "context.hpp"
#include "util.hpp"
#include "val.hpp"

namespace cmcpp
{
    std::vector<Val> lift_values(const CallContext &cx, CoreValueIter &vi, const std::vector<Val> &ts);
    std::vector<Val> lift_values(const CallContext &cx, const std::vector<WasmVal> &vs, const std::vector<Val> &ts);
    Val lift_flat(const CallContext &cx, CoreValueIter &vi, const Val &v);
}

#endif