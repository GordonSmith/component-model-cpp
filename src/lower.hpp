#include "context.hpp"
#include "val.hpp"

#include <type_traits>

namespace cmcpp
{
    std::vector<WasmVal> lower_values(const CallContext &cx, const std::vector<Val> &vs, size_t max_flat = 16, int *out_param = nullptr);

    template <typename T>
    Val lower_hostVal(const CallContext &cx, T hostVal)
    {
        return hostVal;
    }

    template <>
    Val lower_hostVal(const CallContext &cx, std::string hostVal)
    {
        auto retVal = std::make_shared<String>(hostVal);
        return retVal;
    }

    template <typename T>
    Val lower_hostVal(const CallContext &cx, std::vector<T> hostVal)
    {
        auto size = hostVal.size();
        auto lt = ValTrait<T>::type();
        auto retVal = std::make_shared<List>(ValTrait<T>::type());
        for (const auto &val : hostVal)
            retVal->vs.push_back(lower_hostVal(cx, val));
        return retVal;
    }
}
