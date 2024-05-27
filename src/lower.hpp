#ifndef LOWER_HPP
#define LOWER_HPP

#include "context.hpp"
#include "val.hpp"
#include "util.hpp"

// #include <type_traits>

namespace cmcpp
{
    std::vector<WasmVal> lower_values(const CallContext &cx, const std::vector<Val> &vs, std::optional<CoreValueIter> out_param = std::nullopt);

    Val lower_hostVal(const CallContext &cx, std::string hostVal);

    template <typename T>
    inline Val lower_hostVal(const CallContext &cx, T hostVal)
    {
        return hostVal;
    }

    template <>
    inline Val lower_hostVal(const CallContext &cx, std::string hostVal)
    {
        auto retVal = std::make_shared<String>(hostVal);
        return retVal;
    }

    template <typename T>
    inline Val lower_hostVal(const CallContext &cx, std::vector<T> hostVal)
    {
        auto size = hostVal.size();
        auto lt = ValTrait<T>::type();
        auto retVal = std::make_shared<List>(ValTrait<T>::type());
        for (const auto &val : hostVal)
            retVal->vs.push_back(lower_hostVal(cx, val));
        return retVal;
    }
}

#endif