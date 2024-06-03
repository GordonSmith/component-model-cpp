#ifndef FLATTEN_HPP
#define FLATTEN_HPP

#include "context.hpp"
#include "val.hpp"

namespace cmcpp
{
    const int MAX_FLAT_PARAMS = 16;
    const int MAX_FLAT_RESULTS = 1;
    std::vector<std::string> flatten_variant(const std::vector<case_ptr> &cases);
    std::vector<std::string> flatten_type(ValType kind);
    std::vector<std::string> flatten_type(const Val &v);
}

#endif
