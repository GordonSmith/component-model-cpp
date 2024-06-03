#ifndef LIFT_HPP
#define LIFT_HPP

#include "context.hpp"
#include "util.hpp"

namespace cmcpp
{
    Val lift_flat(const CallContext &cx, const CoreValueIter &vi, const Val &t);

}

#endif