#ifndef FLATTEN_HPP
#define FLATTEN_HPP

#include "context.hpp"
#include "val.hpp"

namespace cmcpp
{
    std::vector<std::string> flatten_types(const std::vector<ValType> &ts);
}

#endif
