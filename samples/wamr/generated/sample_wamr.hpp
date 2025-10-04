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

#endif // GENERATED_WAMR_BINDINGS_HPP
