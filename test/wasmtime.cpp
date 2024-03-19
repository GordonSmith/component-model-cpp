#include "context.hpp"
#include "lower.hpp"
#include "lift.hpp"

#include <doctest/doctest.h>

#include <iostream>
#include <string>
#include <fstream>
#include <functional>

#include <wasmtime.hh>

std::vector<uint8_t> readWasmBinaryToBuffer(const char *filename)
{
    std::ifstream file(filename, std::ios::binary);
    std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(file), {});
    return buffer;
}

wasmtime::Val val2WasmtimeVal(const cmcpp::WasmVal &val)
{
    switch (val.kind())
    {
    case cmcpp::WasmValType::I32:
        return val.i32();
    case cmcpp::WasmValType::I64:
        return val.i64();
    case cmcpp::WasmValType::F32:
        return val.f32();
    case cmcpp::WasmValType::F64:
        return val.f64();
    default:
        throw std::runtime_error("Invalid WasmValType");
    }
}

std::vector<wasmtime::Val> vals2WasmtimeVals(const std::vector<cmcpp::WasmVal> &vals)
{
    std::vector<wasmtime::Val> retVal;
    for (const auto &val : vals)
        retVal.push_back(val2WasmtimeVal(val));
    return retVal;
}

cmcpp::WasmVal wasmtimeVal2Val(const wasmtime::Val &val)
{
    switch (val.kind())
    {
    case wasmtime::ValKind::I32:
        return val.i32();
    case wasmtime::ValKind::I64:
        return val.i64();
    case wasmtime::ValKind::F32:
        return val.f32();
    case wasmtime::ValKind::F64:
        return val.f64();
    default:
        throw std::runtime_error("Invalid WasmValType");
    }
}

std::vector<cmcpp::WasmVal> wasmtimeVals2WasmVals(const std::vector<wasmtime::Val> &vals)
{
    std::vector<cmcpp::WasmVal> retVal;
    for (const auto &val : vals)
        retVal.push_back(wasmtimeVal2Val(val));
    return retVal;
}

TEST_CASE("component-model-cpp")
{
    std::cout << "component-model-cpp" << std::endl;

    wasmtime::Engine engine;

    wasmtime::Store store(engine);
    std::vector<uint8_t> _contents = readWasmBinaryToBuffer("wasm/build/wasmtests.wasm");
    const wasmtime::Span<uint8_t> &contents = _contents;
    auto module = wasmtime::Module::compile(engine, contents).unwrap();

    wasmtime::WasiConfig wasi;
    wasi.inherit_argv();
    wasi.inherit_env();
    wasi.inherit_stdin();
    wasi.inherit_stdout();
    wasi.inherit_stderr();
    store.context().set_wasi(std::move(wasi)).unwrap();

    wasmtime::Linker linker(engine);
    linker.define_wasi().unwrap();

    auto dbglog = [&store](wasmtime::Caller caller, uint32_t msg, uint32_t msg_len)
    {
        auto memory = std::get<wasmtime::Memory>(*caller.get_export("memory"));
        auto data = memory.data(store.context());
        const char *msg_ptr = reinterpret_cast<const char *>(&data[msg]);
        std::string str(msg_ptr, msg_len);
        std::cout << str << std::endl;
    };

    linker.func_wrap("$root", "dbglog", dbglog).unwrap();
    auto instance = linker.instantiate(store, module).unwrap();
    linker.define_instance(store, "linking2", instance).unwrap();

    auto cabi_realloc = std::get<wasmtime::Func>(*instance.get(store, "cabi_realloc"));
    std::function<int(int, int, int, int)> realloc = [&store, cabi_realloc](int a, int b, int c, int d) -> int
    {
        return cabi_realloc.call(store, {a, b, c, d}).unwrap()[0].i32();
    };
    auto memory = std::get<wasmtime::Memory>(*instance.get(store, "memory"));
    store.context().set_data(memory);
    wasmtime::Span<uint8_t> data = memory.data(store.context());
    auto bool_test = std::get<wasmtime::Func>(*instance.get(store, "bool-test"));
    auto utf8_string_test = std::get<wasmtime::Func>(*instance.get(store, "utf8-string-test"));

    //  Actual ABI Test Code  --------------------------------------------
    cmcpp::CallContextPtr cx = cmcpp::createCallContext(data, realloc);
    std::vector<wasmtime::Val> wasmtimeVals;
    std::vector<cmcpp::Val> cmcppVals;
    std::vector<cmcpp::WasmVal> cmcppWasmVals;

    wasmtimeVals = vals2WasmtimeVals(cmcpp::lower_values(*cx, {false, false}));
    wasmtimeVals = bool_test.call(store, wasmtimeVals).unwrap();
    cmcppVals = cmcpp::lift_values(*cx, wasmtimeVals2WasmVals(wasmtimeVals), {cmcpp::ValType::Bool});
    CHECK(cmcppVals[0].b() == false);

    wasmtimeVals = vals2WasmtimeVals(cmcpp::lower_values(*cx, {false, true}));
    wasmtimeVals = bool_test.call(store, wasmtimeVals).unwrap();
    cmcppVals = cmcpp::lift_values(*cx, wasmtimeVals2WasmVals(wasmtimeVals), {cmcpp::ValType::Bool});
    CHECK(cmcppVals[0].b() == false);

    wasmtimeVals = vals2WasmtimeVals(cmcpp::lower_values(*cx, {true, false}));
    wasmtimeVals = bool_test.call(store, wasmtimeVals).unwrap();
    cmcppVals = cmcpp::lift_values(*cx, wasmtimeVals2WasmVals(wasmtimeVals), {cmcpp::ValType::Bool});
    CHECK(cmcppVals[0].b() == false);

    wasmtimeVals = vals2WasmtimeVals(cmcpp::lower_values(*cx, {true, true}));
    wasmtimeVals = bool_test.call(store, wasmtimeVals).unwrap();
    cmcppVals = cmcpp::lift_values(*cx, wasmtimeVals2WasmVals(wasmtimeVals), {cmcpp::ValType::Bool});
    CHECK(cmcppVals[0].b() == true);

    wasmtimeVals = vals2WasmtimeVals(cmcpp::lower_values(*cx, {"aaa", "bbb"}));
    wasmtimeVals = utf8_string_test.call(store, wasmtimeVals).unwrap();
    cmcppWasmVals = wasmtimeVals2WasmVals(wasmtimeVals);
    cmcppVals = cmcpp::lift_values(*cx, cmcppWasmVals, {cmcpp::ValType::String});
    std::string str = std::string(cmcppVals[0].s().ptr, cmcppVals[0].s().len);
    CHECK(str.compare("aaabbb") == 0);

    cmcppVals = cmcpp::lift_values(*cx, wasmtimeVals2WasmVals(utf8_string_test.call(store, vals2WasmtimeVals(cmcpp::lower_values(*cx, {"1234", "5678"}))).unwrap()), {cmcpp::ValType::String});
    str = std::string(cmcppVals[0].s().ptr, cmcppVals[0].s().len);
    CHECK(str.compare("12345678") == 0);
    //  Actual ABI Test Code  --------------------------------------------
}

TEST_CASE("component-model-cpp version")
{
    // static_assert(std::string_view(GREETER_VERSION) == std::string_view("1.0"));
    // CHECK(std::string(GREETER_VERSION) == std::string("1.0"));
}
