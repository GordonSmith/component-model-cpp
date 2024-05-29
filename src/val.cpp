#include "val.hpp"

#include <cstring>

namespace cmcpp
{

    ValType type(const ValBase &v)
    {
        return v.t;
    }

    CoreFuncType::CoreFuncType(const std::vector<std::string> &params, const std::vector<std::string> &results)
        : params(params), results(results) {}

}