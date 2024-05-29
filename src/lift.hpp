#ifndef LIFT_HPP
#define LIFT_HPP

#include "context.hpp"
#include "traits.hpp"
#include "util.hpp"

namespace cmcpp
{
    Val lift_flat(const CallContext &cx, const CoreValueIter &vi, ValType t);
}

#endif