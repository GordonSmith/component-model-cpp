#include "context.hpp"
#include "val.hpp"

namespace cmcpp
{
    std::vector<WasmVal> lower_values(const CallContext &cx, const std::vector<Val> &vs, size_t max_flat = 16, int *out_param = nullptr);
}
