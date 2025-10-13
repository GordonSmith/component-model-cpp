#include "sample_wamr.hpp"

#include <stdexcept>
#include <vector>

// Generated WAMR bindings for package: example:sample
// These symbol arrays can be used with wasm_runtime_register_natives_raw()
// NOTE: You must implement the functions declared in the host namespace
// (See sample.hpp for declarations, provide implementations in your host code)

using namespace cmcpp;

// WAMR Native Symbol arrays organized by interface
// Register these with wasm_runtime_register_natives_raw(namespace, array, count)

// Import interface: booleans
// Register with: wasm_runtime_register_natives_raw("example:sample/booleans", booleans_symbols, 1)
NativeSymbol booleans_symbols[] = {
    host_function("and", host::booleans::and_),
};

// Import interface: floats
// Register with: wasm_runtime_register_natives_raw("example:sample/floats", floats_symbols, 1)
NativeSymbol floats_symbols[] = {
    host_function("add", host::floats::add),
};

// Import interface: strings
// Register with: wasm_runtime_register_natives_raw("example:sample/strings", strings_symbols, 2)
NativeSymbol strings_symbols[] = {
    host_function("reverse", host::strings::reverse),
    host_function("lots", host::strings::lots),
};

// Import interface: lists
// Register with: wasm_runtime_register_natives_raw("example:sample/lists", lists_symbols, 1)
NativeSymbol lists_symbols[] = {
    host_function("filter-bool", host::lists::filter_bool),
};

// Import interface: logging
// Register with: wasm_runtime_register_natives_raw("example:sample/logging", logging_symbols, 6)
NativeSymbol logging_symbols[] = {
    host_function("log-bool", host::logging::log_bool),
    host_function("log-u32", host::logging::log_u32),
    host_function("log-u64", host::logging::log_u64),
    host_function("log-f32", host::logging::log_f32),
    host_function("log-f64", host::logging::log_f64),
    host_function("log-str", host::logging::log_str),
};

// Import interface: void-func
// Register with: wasm_runtime_register_natives_raw("$root", void_func_symbols, 1)
NativeSymbol void_func_symbols[] = {
    host_function("void-func", host::void_func),
};

// Get all import interfaces for registration
// Usage:
//   for (const auto& reg : get_import_registrations()) {
//       wasm_runtime_register_natives_raw(reg.module_name, reg.symbols, reg.count);
//   }
std::vector<NativeRegistration> get_import_registrations() {
    return {
        {"example:sample/booleans", booleans_symbols, 1},
        {"example:sample/floats", floats_symbols, 1},
        {"example:sample/strings", strings_symbols, 2},
        {"example:sample/lists", lists_symbols, 1},
        {"example:sample/logging", logging_symbols, 6},
        {"$root", void_func_symbols, 1},
    };
}

// Helper function to register all import interfaces at once
// Returns the number of functions registered
int register_all_imports() {
    int count = 0;
    for (const auto& reg : get_import_registrations()) {
        if (!wasm_runtime_register_natives_raw(reg.module_name, reg.symbols, reg.count)) {
            return -1;  // Registration failed
        }
        count += reg.count;
    }
    return count;
}

// Helper function to unregister all import interfaces
void unregister_all_imports() {
    for (const auto& reg : get_import_registrations()) {
        wasm_runtime_unregister_natives(reg.module_name, reg.symbols);
    }
}

// WASM file utilities
namespace wasm_utils {

const uint32_t DEFAULT_STACK_SIZE = 8192;
const uint32_t DEFAULT_HEAP_SIZE = 8192;

} // namespace wasm_utils
