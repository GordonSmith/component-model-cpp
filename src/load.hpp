#ifndef LOAD_HPP
#define LOAD_HPP

#include "context.hpp"
#include "val.hpp"

namespace cmcpp
{
    std::shared_ptr<string_t> load_string(const CallContext &cx, uint32_t ptr);
    std::shared_ptr<string_t> load_string_from_range(const CallContext &cx, uint32_t ptr, uint32_t tagged_code_units);
    std::shared_ptr<list_t> load_list_from_range(const CallContext &cx, uint32_t ptr, uint32_t length, const Val &t);
    Val load(const CallContext &cx, uint32_t ptr, ValType t);
    Val load(const CallContext &cx, uint32_t ptr, const Val &v);
}
#endif