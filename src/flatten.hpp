#ifndef FLATTEN_HPP
#define FLATTEN_HPP

#include "context.hpp"
#include "val.hpp"

namespace cmcpp
{
    const int MAX_FLAT_PARAMS = 16;
    const int MAX_FLAT_RESULTS = 1;

    std::vector<std::string> flatten_types(const std::vector<Val> &vs);
    std::vector<std::string> flatten_types(const std::vector<ValType> &ts);
    std::vector<std::string> flatten_types(const std::vector<WasmVal> &ts);
    std::vector<std::string> flatten_variant(const std::vector<Case> &cases);
    std::vector<std::string> flatten_record(const std::vector<Field> &fields);
}

#endif
