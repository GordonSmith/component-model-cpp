#ifndef FLATTEN_HPP
#define FLATTEN_HPP

#include "context.hpp"
#include "val.hpp"

namespace cmcpp
{
    std::vector<std::string> flatten_types(const std::vector<Val> &vs);
    std::vector<std::string> flatten_types(const std::vector<ValType> &ts);
    std::vector<std::string> flatten_variant(const std::vector<Case> &cases);
}

#endif
