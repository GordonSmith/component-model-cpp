#ifndef LOAD_HPP
#define LOAD_HPP

#include "context.hpp"
#include "val.hpp"

namespace cmcpp
{
    string_ptr load_string(const LiftLowerContext &cx, uint32_t ptr);
    string_ptr load_string_from_range(const LiftLowerContext &cx, uint32_t ptr, uint32_t tagged_code_units);
    std::shared_ptr<list_t> load_list_from_range(const LiftLowerContext &cx, uint32_t ptr, uint32_t length, const Val &t);
    std::string case_label_with_refinements(case_ptr c, const std::vector<case_ptr> &cases);
    Val load(const LiftLowerContext &cx, uint32_t ptr, ValType t);
    Val load(const LiftLowerContext &cx, uint32_t ptr, const Val &v);
}
#endif