#ifndef LIFT_HPP
#define LIFT_HPP

#include "context.hpp"
#include "val.hpp"

namespace cmcpp
{
    class CoreValueIter
    {

    public:
        std::vector<std::variant<int32_t, int64_t, float32_t, float64_t>> values;
        size_t i = 0;

        CoreValueIter(const std::vector<std::variant<int32_t, int64_t, float32_t, float64_t>> &values);

        template <typename T>
        T next()
        {
            auto v = values[i];
            i++;
            // assert(std::holds_alternative<T>(v));
            return std::get<T>(v);
        }
        void skip();
    };

    using WasmVal = std::variant<int32_t, int64_t, float32_t, float64_t>;
    std::vector<Val> lift_values(const CallContext &cx, int max_flat, CoreValueIter &vi, const std::vector<Val> &ts);
    // std::vector<Val> lift_values(const CallContext &cx, const std::vector<WasmVal> &vs, const std::vector<std::pair<ValType, ValType>> &ts, int max_flat = 16);
    Val lift_flat(const CallContext &cx, CoreValueIter &vi, const Val &v);
}

#endif