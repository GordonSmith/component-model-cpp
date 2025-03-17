#include <iostream>

#include "wasm_export.h"
#include "cmcpp.hpp"

using namespace cmcpp;

char *read_wasm_binary_to_buffer(const char *filename, uint32_t *size)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return nullptr;
    }

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = new char[*size];
    if (fread(buffer, 1, *size, file) != *size)
    {
        std::cerr << "Failed to read file: " << filename << std::endl;
        delete[] buffer;
        fclose(file);
        return nullptr;
    }

    fclose(file);
    return buffer;
}

void trap(const char *msg)
{
    throw new std::runtime_error(msg);
}

std::pair<void *, size_t> convert(void *dest, uint32_t dest_byte_len, const void *src, uint32_t src_byte_len, Encoding from_encoding, Encoding to_encoding)
{
    if (from_encoding == to_encoding)
    {
        assert(dest_byte_len >= src_byte_len);
        if (src_byte_len > 0)
        {
            std::memcpy(dest, src, src_byte_len);
            return std::make_pair(dest, src_byte_len);
        }
        return std::make_pair(nullptr, 0);
    }
    assert(false);
}

int main()
{
    std::cout << "Hello, World!" << std::endl;

    char *buffer, error_buf[128];
    wasm_module_t module;
    wasm_module_inst_t module_inst;
    wasm_function_inst_t cabi_realloc, func;
    wasm_exec_env_t exec_env;
    uint32_t size, stack_size = 8092, heap_size = 8092;

    /* initialize the wasm runtime by default configurations */
    wasm_runtime_init();

    /* read WASM file into a memory buffer */
    buffer = read_wasm_binary_to_buffer("build/samples/wasm/src/wasm-build/sample.wasm", &size);

    // /* add line below if we want to export native functions to WASM app */
    // wasm_runtime_register_natives(...);

    // /* parse the WASM file from buffer and create a WASM module */
    module = wasm_runtime_load((uint8_t *)buffer, size, error_buf, sizeof(error_buf));

    // /* create an instance of the WASM module (WASM linear memory is ready) */
    module_inst = wasm_runtime_instantiate(module, stack_size, heap_size, error_buf, sizeof(error_buf));

    wasm_memory_inst_t memory = wasm_runtime_lookup_memory(module_inst, "memory");
    cabi_realloc = wasm_runtime_lookup_function(module_inst, "cabi_realloc");

    // auto cx = createCallContext(&heap, Encoding::Utf8);

    exec_env = wasm_runtime_create_exec_env(module_inst, stack_size);

    GuestRealloc realloc = [exec_env, cabi_realloc](int original_ptr, int original_size, int alignment, int new_size) -> int
    {
        uint32_t argv[4];
        argv[0] = original_ptr;
        argv[1] = original_size;
        argv[2] = alignment;
        argv[3] = new_size;
        wasm_runtime_call_wasm(exec_env, cabi_realloc, 4, argv);
        return argv[0];
    };

    CallContext callContext = {
        .trap = trap,
        .realloc = realloc,
        .memory = std::span<uint8_t>(static_cast<uint8_t *>(wasm_memory_get_base_address(memory)), 999999),
        .guest_encoding = Encoding::Utf8,
        .convert = convert,
        .post_return = nullptr,
    };

    func = wasm_runtime_lookup_function(module_inst, "example:sample/boolean#and");

    // bool_t arg1 = true;
    // bool_t arg2 = false;
    // auto args_flat = cmcpp::lower_flat(callContext, args);
    uint32_t argv[2] = {true, false};

    list_t<variant_t<bool_t>> args = {true, false};
    auto args_flat = cmcpp::lower_flat(callContext, args);
    auto call_result = wasm_runtime_call_wasm(exec_env, func, 2, (uint32_t *)args_flat.data());
    args_flat.resize(1);
    auto result = cmcpp::lift_flat<bool_t>(callContext, args_flat);
    std::cout << "Result: " << result << std::endl;

    wasm_runtime_destroy_exec_env(exec_env);
    wasm_runtime_deinstantiate(module_inst);
    wasm_runtime_unload(module);
    wasm_runtime_destroy();

    return 0;
}