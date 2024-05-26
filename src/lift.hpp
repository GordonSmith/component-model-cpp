#ifndef LIFT_HPP
#define LIFT_HPP

#include "context.hpp"
#include "val.hpp"

namespace cmcpp
{
    std::vector<Val> lift_values(const CallContext &cx, const std::vector<WasmVal> &vs, const std::vector<ValType> &ts, int max_flat = 16);
    std::vector<Val> lift_values(const CallContext &cx, const std::vector<WasmVal> &vs, const std::vector<std::pair<ValType, ValType>> &ts, int max_flat = 16);
    Val lift_flat(const CallContext &cx, const WasmVal &v, const ValType &t, const ValType &lt = ValType::Unknown);

    class CoreValueIter
    {

    public:
        std::vector<WasmVal> values;
        size_t i = 0;

        CoreValueIter(const std::vector<std::variant<int32_t, float32_t>> &values);

        std::variant<int32_t, float32_t> next(WasmValType t);
    };
}

#endif