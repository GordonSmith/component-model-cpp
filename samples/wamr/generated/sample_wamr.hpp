#ifndef GENERATED_WAMR_BINDINGS_HPP
#define GENERATED_WAMR_BINDINGS_HPP

// Generated WAMR helper functions for package: example:sample
// This header provides utility functions for initializing and using WAMR with Component Model bindings

#include <wamr.hpp>
#include <cmcpp.hpp>
#include "sample.hpp"

#include <span>
#include <stdexcept>
#include <vector>

// Forward declarations
struct NativeSymbol;
struct NativeRegistration {
    const char* module_name;
    NativeSymbol* symbols;
    size_t count;
};

// Get all import interface registrations
// Returns a vector of all import interfaces that need to be registered with WAMR
std::vector<NativeRegistration> get_import_registrations();

// Register all import interfaces at once
// Returns the number of functions registered, or -1 on failure
int register_all_imports();

// Unregister all import interfaces
void unregister_all_imports();

// WASM file utilities
namespace wasm_utils {

// Default WAMR runtime configuration
extern const uint32_t DEFAULT_STACK_SIZE;
extern const uint32_t DEFAULT_HEAP_SIZE;

} // namespace wasm_utils

// Note: Helper functions create_guest_realloc() and create_lift_lower_context()
// are now available directly from <wamr.hpp> in the cmcpp namespace

// ==============================================================================
// Guest Function Wrappers (Exports - Guest implements, Host calls)
// ==============================================================================
// These functions create pre-configured guest function wrappers for all exported
// functions from the guest module. Use these instead of manually calling
// guest_function<T>() with the export name.

namespace guest_wrappers {

// Interface: example:sample/booleans
namespace booleans {
    inline auto and_(const wasm_module_inst_t& module_inst, const wasm_exec_env_t& exec_env, 
                         cmcpp::LiftLowerContext& ctx) {
        return cmcpp::guest_function<::guest::booleans::and_t>(
            module_inst, exec_env, ctx, "example:sample/booleans#and");
    }
} // namespace booleans

// Interface: example:sample/floats
namespace floats {
    inline auto add(const wasm_module_inst_t& module_inst, const wasm_exec_env_t& exec_env, 
                        cmcpp::LiftLowerContext& ctx) {
        return cmcpp::guest_function<::guest::floats::add_t>(
            module_inst, exec_env, ctx, "example:sample/floats#add");
    }
} // namespace floats

// Interface: example:sample/strings
namespace strings {
    inline auto reverse(const wasm_module_inst_t& module_inst, const wasm_exec_env_t& exec_env, 
                            cmcpp::LiftLowerContext& ctx) {
        return cmcpp::guest_function<::guest::strings::reverse_t>(
            module_inst, exec_env, ctx, "example:sample/strings#reverse");
    }
    inline auto lots(const wasm_module_inst_t& module_inst, const wasm_exec_env_t& exec_env, 
                         cmcpp::LiftLowerContext& ctx) {
        return cmcpp::guest_function<::guest::strings::lots_t>(
            module_inst, exec_env, ctx, "example:sample/strings#lots");
    }
} // namespace strings

// Interface: example:sample/tuples
namespace tuples {
    inline auto reverse(const wasm_module_inst_t& module_inst, const wasm_exec_env_t& exec_env, 
                            cmcpp::LiftLowerContext& ctx) {
        return cmcpp::guest_function<::guest::tuples::reverse_t>(
            module_inst, exec_env, ctx, "example:sample/tuples#reverse");
    }
} // namespace tuples

// Interface: example:sample/lists
namespace lists {
    inline auto filter_bool(const wasm_module_inst_t& module_inst, const wasm_exec_env_t& exec_env, 
                                cmcpp::LiftLowerContext& ctx) {
        return cmcpp::guest_function<::guest::lists::filter_bool_t>(
            module_inst, exec_env, ctx, "example:sample/lists#filter-bool");
    }
} // namespace lists

// Interface: example:sample/variants
namespace variants {
    inline auto variant_func(const wasm_module_inst_t& module_inst, const wasm_exec_env_t& exec_env, 
                                 cmcpp::LiftLowerContext& ctx) {
        return cmcpp::guest_function<::guest::variants::variant_func_t>(
            module_inst, exec_env, ctx, "example:sample/variants#variant-func");
    }
} // namespace variants

// Interface: example:sample/enums
namespace enums {
    inline auto enum_func(const wasm_module_inst_t& module_inst, const wasm_exec_env_t& exec_env, 
                              cmcpp::LiftLowerContext& ctx) {
        return cmcpp::guest_function<::guest::enums::enum_func_t>(
            module_inst, exec_env, ctx, "example:sample/enums#enum-func");
    }
} // namespace enums

// Standalone function: void-func
inline auto void_func(const wasm_module_inst_t& module_inst, const wasm_exec_env_t& exec_env, 
                            cmcpp::LiftLowerContext& ctx) {
    return cmcpp::guest_function<::guest::void_func_t>(
        module_inst, exec_env, ctx, "void-func");
}

// Standalone function: ok-func
inline auto ok_func(const wasm_module_inst_t& module_inst, const wasm_exec_env_t& exec_env, 
                          cmcpp::LiftLowerContext& ctx) {
    return cmcpp::guest_function<::guest::ok_func_t>(
        module_inst, exec_env, ctx, "ok-func");
}

// Standalone function: err-func
inline auto err_func(const wasm_module_inst_t& module_inst, const wasm_exec_env_t& exec_env, 
                           cmcpp::LiftLowerContext& ctx) {
    return cmcpp::guest_function<::guest::err_func_t>(
        module_inst, exec_env, ctx, "err-func");
}

// Standalone function: option-func
inline auto option_func(const wasm_module_inst_t& module_inst, const wasm_exec_env_t& exec_env, 
                              cmcpp::LiftLowerContext& ctx) {
    return cmcpp::guest_function<::guest::option_func_t>(
        module_inst, exec_env, ctx, "option-func");
}

} // namespace guest_wrappers

#endif // GENERATED_WAMR_BINDINGS_HPP
