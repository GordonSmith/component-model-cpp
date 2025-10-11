#include "generated/sample_wamr.hpp" // Includes wamr.hpp, sample.hpp and all helpers
#include <algorithm>
#include <cfloat>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <system_error>
#include <tuple>

using namespace cmcpp;

// WASM file location
const std::filesystem::path WASM_RELATIVE_PATH = std::filesystem::path("bin") / "sample.wasm";

std::filesystem::path resolve_wasm_path(const std::filesystem::path &start_dir)
{
    constexpr int MAX_ASCENT = 6;
    auto dir = start_dir;
    for (int depth = 0; depth < MAX_ASCENT && !dir.empty(); ++depth)
    {
        const auto candidate = dir / WASM_RELATIVE_PATH;
        if (std::filesystem::exists(candidate))
        {
            std::error_code ec;
            const auto normalized = std::filesystem::weakly_canonical(candidate, ec);
            return ec ? candidate : normalized;
        }
        if (!dir.has_parent_path())
        {
            break;
        }
        dir = dir.parent_path();
    }

    return {};
}

char *read_wasm_binary_to_buffer(const std::filesystem::path &filename, uint32_t *size)
{
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file)
    {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return nullptr;
    }

    const auto file_size = file.tellg();
    if (file_size < 0)
    {
        std::cerr << "Failed to determine file size: " << filename << std::endl;
        return nullptr;
    }

    if (file_size > static_cast<std::streamoff>(std::numeric_limits<uint32_t>::max()))
    {
        std::cerr << "File too large to load: " << filename << std::endl;
        return nullptr;
    }

    *size = static_cast<uint32_t>(file_size);
    file.seekg(0, std::ios::beg);

    char *buffer = new char[*size];
    if (!file.read(buffer, static_cast<std::streamsize>(*size)))
    {
        std::cerr << "Failed to read file: " << filename << std::endl;
        delete[] buffer;
        return nullptr;
    }

    return buffer;
}

int main(int argc, char **argv)
{
    static_cast<void>(argc);

    std::cout << "WAMR Component Model C++ Sample (Using Generated Code)" << std::endl;
    std::cout << "======================================================" << std::endl;
    std::cout << "Starting WAMR runtime initialization..." << std::endl;

    std::filesystem::path exe_dir = std::filesystem::current_path();
    if (argv && argv[0])
    {
        const std::filesystem::path candidate = std::filesystem::absolute(argv[0]);
        if (std::filesystem::exists(candidate))
        {
            exe_dir = candidate.parent_path();
        }
    }

    std::filesystem::path wasm_path = resolve_wasm_path(exe_dir);
    if (wasm_path.empty())
    {
        wasm_path = resolve_wasm_path(std::filesystem::current_path());
    }

    if (wasm_path.empty())
    {
        std::cerr << "Unable to locate sample.wasm relative to " << exe_dir << std::endl;
        return 1;
    }

    std::cout << "Using WASM file: " << wasm_path << std::endl;

    char *buffer, error_buf[128];
    wasm_module_t module;
    wasm_module_inst_t module_inst;
    wasm_function_inst_t cabi_realloc;
    wasm_exec_env_t exec_env;
    uint32_t size, stack_size = wasm_utils::DEFAULT_STACK_SIZE, heap_size = wasm_utils::DEFAULT_HEAP_SIZE;

    /* initialize the wasm runtime by default configurations */
    wasm_runtime_init();
    std::cout << "WAMR runtime initialized successfully" << std::endl;

    /* read WASM file into a memory buffer */
    buffer = read_wasm_binary_to_buffer(wasm_path, &size);
    if (!buffer)
    {
        std::cerr << "Failed to read WASM file" << std::endl;
        return 1;
    }
    std::cout << "Successfully loaded WASM file (" << size << " bytes)" << std::endl;

    // Register native functions using generated code
    // Note: We only register functions that are IMPORTED by the guest (what the host provides)
    std::cout << "\n=== Registering Host Functions (Imports) ===" << std::endl;

    // Use the generated helper to register all interface imports
    int registered_count = register_all_imports();
    if (registered_count < 0)
    {
        std::cerr << "Failed to register import interfaces" << std::endl;
        return 1;
    }

    std::cout << "\nTotal: " << (registered_count + 1) << " host functions registered (all imports from guest perspective)" << std::endl;

    // Parse the WASM file from buffer and create a WASM module
    module = wasm_runtime_load((uint8_t *)buffer, size, error_buf, sizeof(error_buf));
    if (!module)
    {
        std::cerr << "Failed to load WASM module: " << error_buf << std::endl;
        delete[] buffer;
        wasm_runtime_destroy();
        return 1;
    }
    std::cout << "\nSuccessfully loaded WASM module" << std::endl;

    // Create an instance of the WASM module (WASM linear memory is ready)
    module_inst = wasm_runtime_instantiate(module, stack_size, heap_size, error_buf, sizeof(error_buf));
    if (!module_inst)
    {
        std::cerr << "Failed to instantiate WASM module: " << error_buf << std::endl;
        wasm_runtime_unload(module);
        delete[] buffer;
        wasm_runtime_destroy();
        return 1;
    }
    std::cout << "Successfully instantiated WASM module" << std::endl;

    cabi_realloc = wasm_runtime_lookup_function(module_inst, "cabi_realloc");
    if (!cabi_realloc)
    {
        std::cerr << "Failed to lookup cabi_realloc function" << std::endl;
        wasm_runtime_deinstantiate(module_inst);
        wasm_runtime_unload(module);
        delete[] buffer;
        wasm_runtime_destroy();
        return 1;
    }

    exec_env = wasm_runtime_create_exec_env(module_inst, stack_size);

    // Use the helper from wamr.hpp to create the LiftLowerContext
    LiftLowerContext liftLowerContext = cmcpp::create_lift_lower_context(module_inst, exec_env, cabi_realloc);

    std::cout << "\n=== Testing Guest Functions (Exports) ===" << std::endl;
    std::cout << "Note: These are functions the GUEST implements, HOST calls them\n"
              << std::endl;

    // Using generated typedefs from sample_host.hpp for guest exports
    std::cout << "\n--- Boolean Functions (Guest Export) ---" << std::endl;
    auto call_and = guest_function<guest::booleans::and_t>(module_inst, exec_env, liftLowerContext,
                                                           "example:sample/booleans#and");
    std::cout << "call_and(false, false): " << call_and(false, false) << std::endl;
    std::cout << "call_and(false, true): " << call_and(false, true) << std::endl;
    std::cout << "call_and(true, false): " << call_and(true, false) << std::endl;
    std::cout << "call_and(true, true): " << call_and(true, true) << std::endl;

    std::cout << "\n--- Float Functions ---" << std::endl;
    auto call_add = guest_function<guest::floats::add_t>(module_inst, exec_env, liftLowerContext,
                                                         "example:sample/floats#add");
    std::cout << "call_add(3.1, 0.2): " << call_add(3.1, 0.2) << std::endl;
    std::cout << "call_add(1.5, 2.5): " << call_add(1.5, 2.5) << std::endl;
    std::cout << "call_add(DBL_MAX, 0.0): " << call_add(DBL_MAX / 2, 0.0) << std::endl;
    std::cout << "call_add(DBL_MAX / 2, DBL_MAX / 2): " << call_add(DBL_MAX / 2, DBL_MAX / 2) << std::endl;

    std::cout << "\n--- String Functions ---" << std::endl;
    auto call_reverse = guest_function<guest::strings::reverse_t>(module_inst, exec_env, liftLowerContext,
                                                                  "example:sample/strings#reverse");
    auto call_reverse_result = call_reverse("Hello World!");
    std::cout << "reverse(\"Hello World!\"): " << call_reverse_result << std::endl;
    std::cout << "reverse(reverse(\"Hello World!\")): " << call_reverse(call_reverse_result) << std::endl;

    std::cout << "\n--- Variant Functions ---" << std::endl;
    auto variant_func = guest_function<guest::variants::variant_func_t>(
        module_inst, exec_env, liftLowerContext, "example:sample/variants#variant-func");
    std::cout << "variant_func((uint32_t)40): " << std::get<1>(variant_func((uint32_t)40)) << std::endl;
    std::cout << "variant_func((bool_t)true): " << std::get<0>(variant_func((bool_t) true)) << std::endl;
    std::cout << "variant_func((bool_t)false): " << std::get<0>(variant_func((bool_t) false)) << std::endl;

    std::cout << "\n--- Option Functions ---" << std::endl;
    auto option_func = guest_function<guest::option_func_t>(
        module_inst, exec_env, liftLowerContext, "option-func");
    std::cout << "option_func((uint32_t)40).has_value(): " << option_func((uint32_t)40).has_value() << std::endl;
    std::cout << "option_func((uint32_t)40).value(): " << option_func((uint32_t)40).value() << std::endl;
    std::cout << "option_func(std::nullopt).has_value(): " << option_func(std::nullopt).has_value() << std::endl;

    std::cout << "\n--- Void Functions ---" << std::endl;
    auto void_func_guest = guest_function<guest::void_func_t>(module_inst, exec_env, liftLowerContext, "void-func");
    void_func_guest();

    std::cout << "\n--- Result Functions ---" << std::endl;
    auto ok_func = guest_function<guest::ok_func_t>(
        module_inst, exec_env, liftLowerContext, "ok-func");
    auto ok_result = ok_func(40, 2);
    std::cout << "ok_func result: " << std::get<uint32_t>(ok_result) << std::endl;

    auto err_func = guest_function<guest::err_func_t>(
        module_inst, exec_env, liftLowerContext, "err-func");
    auto err_result = err_func(40, 2);
    std::cout << "err_func result: " << std::get<string_t>(err_result) << std::endl;

    std::cout << "\n--- Complex String Functions ---" << std::endl;
    auto call_lots = guest_function<guest::strings::lots_t>(
        module_inst, exec_env, liftLowerContext,
        "example:sample/strings#lots");

    auto call_lots_result = call_lots(
        "p1", "p2", "p3", "p4", "p5", "p6", "p7", "p8",
        "p9", "p10", "p11", "p12", "p13", "p14", "p15", "p16", "p17");
    std::cout << "call_lots result: " << call_lots_result << std::endl;

    std::cout << "\n--- Tuple Functions ---" << std::endl;
    auto call_reverse_tuple = guest_function<guest::tuples::reverse_t>(module_inst, exec_env, liftLowerContext,
                                                                       "example:sample/tuples#reverse");
    auto call_reverse_tuple_result = call_reverse_tuple({false, "Hello World!"});
    std::cout << "call_reverse_tuple({false, \"Hello World!\"}): " << std::get<0>(call_reverse_tuple_result) << ", " << std::get<1>(call_reverse_tuple_result) << std::endl;

    std::cout << "\n--- List Functions ---" << std::endl;
    auto call_list_filter = guest_function<guest::lists::filter_bool_t>(module_inst, exec_env, liftLowerContext, "example:sample/lists#filter-bool");
    auto call_list_filter_result = call_list_filter({{false}, {"Hello World!"}, {"Another String"}, {true}, {false}});
    std::cout << "call_list_filter result: " << call_list_filter_result.size() << std::endl;

    std::cout << "\n--- Enum Functions ---" << std::endl;
    using e = guest::enums::e;

    auto enum_func = guest_function<guest::enums::enum_func_t>(module_inst, exec_env, liftLowerContext, "example:sample/enums#enum-func");
    std::cout << "enum_func(e::a): " << enum_func(static_cast<enum_t<e>>(e::a)) << std::endl;
    std::cout << "enum_func(e::b): " << enum_func(static_cast<enum_t<e>>(e::b)) << std::endl;
    std::cout << "enum_func(e::c): " << enum_func(static_cast<enum_t<e>>(e::c)) << std::endl;

    std::cout << "\n=== Test Complete ===" << std::endl;

    std::cout << "\n=== Cleanup and Summary ===" << std::endl;
    // Clean up resources
    delete[] buffer;
    wasm_runtime_destroy_exec_env(exec_env);

    // Unregister all interface imports using the generated helper
    unregister_all_imports();

    wasm_runtime_deinstantiate(module_inst);
    wasm_runtime_unload(module);
    wasm_runtime_destroy();

    std::cout << "Sample completed successfully!" << std::endl;
    return 0;
}
