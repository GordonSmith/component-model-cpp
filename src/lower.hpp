#ifndef LOWER_HPP
#define LOWER_HPP

#include "context.hpp"
#include "val.hpp"
#include "util.hpp"

namespace cmcpp
{
    std::vector<WasmVal> lower_flat(const CallContext &cx, const Val &v, const Val &t);
}

#endif