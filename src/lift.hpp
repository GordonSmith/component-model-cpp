#ifndef LIFT_HPP
#define LIFT_HPP

#include "context.hpp"
#include "util.hpp"

namespace cmcpp
{
    flags_ptr unpack_flags_from_int(uint64_t i, const std::vector<std::string> &labels);
    Val lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi, const Val &t);
}

#endif