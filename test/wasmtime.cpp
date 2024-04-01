#include "context.hpp"
#include "lower.hpp"
#include "lift.hpp"
#include "val.hpp"
#include "util.hpp"

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
    switch (val.kind)
    {
    case cmcpp::WasmValType::I32:
        return (int32_t)val;
    case cmcpp::WasmValType::I64:
        return (int64_t)val;
    case cmcpp::WasmValType::F32:
        return (float32_t)val;
    case cmcpp::WasmValType::F64:
        return (float64_t)val;
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

std::ostream &operator<<(std::ostream &os, const cmcpp::ValType &valType)
{
    os << static_cast<int>(valType);
    return os;
}

TEST_CASE("generic Val")
{
    // auto a = cmcpp::HostVal(std::vector<int>{1, 2, 3});
    // auto b = cmcpp::HostVal(std::map<std::string, std::vector<int>>{std::make_pair("a", std::vector<int>{1, 2, 3})});
    // // auto b = std::vector<int>{1, 2, 3};
    // std::cout << b.type() << std::endl;
}

cmcpp::ListPtr createList()
{
    auto list = std::make_shared<cmcpp::List>(cmcpp::ValType::Char);
    list->vs.push_back('a');
    list->vs.push_back('b');
    list->vs.push_back('c');
    return list;
}

TEST_CASE("wasmtime")
{
    std::cout << "wasmtime" << std::endl;

    wasmtime::Engine engine;

    wasmtime::Store store(engine);
    std::vector<uint8_t> _contents = readWasmBinaryToBuffer("test/wasm/build/wasmtests.wasm");
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
    auto bool_and = std::get<wasmtime::Func>(*instance.get(store, "bool-and"));
    auto list_char_append = std::get<wasmtime::Func>(*instance.get(store, "list-char-append"));
    auto list_list_string_append = std::get<wasmtime::Func>(*instance.get(store, "list-list-string-append"));
    auto string_append = std::get<wasmtime::Func>(*instance.get(store, "string-append"));

    //  Actual ABI Test Code  --------------------------------------------
    cmcpp::ListPtr list = createList();
    CHECK(list->vs.size() == 3);
    cmcpp::Val v = list;
    auto list3 = std::get<cmcpp::ListPtr>(v);
    CHECK(list3->vs.size() == 3);

    cmcpp::CallContextPtr cx = cmcpp::createCallContext(data, realloc, cmcpp::encodeTo);
    std::vector<wasmtime::Val> wasmtimeVals;
    std::vector<cmcpp::Val> cmcppVals;
    std::vector<cmcpp::WasmVal> cmcppWasmVals;

    wasmtimeVals = vals2WasmtimeVals(cmcpp::lower_values(*cx, {false, false}));
    wasmtimeVals = bool_and.call(store, wasmtimeVals).unwrap();
    cmcppVals = cmcpp::lift_values(*cx, wasmtimeVals2WasmVals(wasmtimeVals), {cmcpp::ValType::Bool});
    CHECK(std::get<bool>(cmcppVals[0]) == false);

    wasmtimeVals = vals2WasmtimeVals(cmcpp::lower_values(*cx, {false, true}));
    wasmtimeVals = bool_and.call(store, wasmtimeVals).unwrap();
    cmcppVals = cmcpp::lift_values(*cx, wasmtimeVals2WasmVals(wasmtimeVals), {cmcpp::ValType::Bool});
    CHECK(std::get<bool>(cmcppVals[0]) == false);

    wasmtimeVals = vals2WasmtimeVals(cmcpp::lower_values(*cx, {true, false}));
    wasmtimeVals = bool_and.call(store, wasmtimeVals).unwrap();
    cmcppVals = cmcpp::lift_values(*cx, wasmtimeVals2WasmVals(wasmtimeVals), {cmcpp::ValType::Bool});
    CHECK(std::get<bool>(cmcppVals[0]) == false);

    wasmtimeVals = vals2WasmtimeVals(cmcpp::lower_values(*cx, {true, true}));
    wasmtimeVals = bool_and.call(store, wasmtimeVals).unwrap();
    cmcppVals = cmcpp::lift_values(*cx, wasmtimeVals2WasmVals(wasmtimeVals), {cmcpp::ValType::Bool});
    CHECK(std::get<bool>(cmcppVals[0]) == true);

    auto myVec = std::vector<char>{'a', 'b', 'c'};
    auto charList1 = cmcpp::lower_hostVal(*cx, myVec);
    auto myVec2 = std::vector<char>{'d', 'e', 'f'};
    auto charList2 = cmcpp::lower_hostVal(*cx, myVec2);
    // auto charList2 = cmcpp::lower_hostVal(std::vector<char>{'d', 'e', 'f'});

    // wasmtimeVals = vals2WasmtimeVals({charList1, charList2});
    // wasmtimeVals = list_char_append.call(store, wasmtimeVals).unwrap();
    // cmcppWasmVals = wasmtimeVals2WasmVals(wasmtimeVals);
    // cmcppVals = cmcpp::lift_values(*cx, cmcppWasmVals, {std::make_pair(cmcpp::ValType::List, cmcpp::ValType::Char)});
    // CHECK(std::get<cmcpp::ListPtr>(cmcppVals[0])->vs.size() == 6);
    // CHECK(std::get<char>(std::get<cmcpp::ListPtr>(cmcppVals[0])->vs[0]) == 'a');
    // CHECK(std::get<char>(std::get<cmcpp::ListPtr>(cmcppVals[0])->vs[3]) == 'd');
    // CHECK(std::get<char>(std::get<cmcpp::ListPtr>(cmcppVals[0])->vs[5]) == 'f');

    auto lvs = cmcpp::lower_values(*cx, {"1234", "5678"});
    auto ret = string_append.call(store, vals2WasmtimeVals(lvs)).unwrap();
    auto wret = wasmtimeVals2WasmVals(ret);
    cmcppVals = cmcpp::lift_values(*cx, wret, {cmcpp::ValType::String});
    auto str = std::get<cmcpp::StringPtr>(cmcppVals[0]);
    CHECK(str->to_string().compare("12345678") == 0);

    // auto myVec4 = std::vector<const std::string &>{{"xxx1234", "xxxx5678", "xxxxxabcd", "xxxxxxefgh"}};
    // auto charListList4 = cmcpp::lower_hostVal(*cx, myVec4);
    // lvs = cmcpp::lower_values(*cx, {charListList4});
    auto myVec3 = std::vector<std::vector<std::string>>{{"1234", "5678"}, {"abcd", "efgh"}};
    auto charListList = cmcpp::lower_hostVal(*cx, myVec3);
    lvs = cmcpp::lower_values(*cx, {charListList, charListList});
    wasmtimeVals = vals2WasmtimeVals(lvs);
    wasmtimeVals = list_list_string_append.call(store, wasmtimeVals).unwrap();
    wret = wasmtimeVals2WasmVals(wasmtimeVals);
    cmcppVals = cmcpp::lift_values(*cx, wret, {cmcpp::ValType::String});
    CHECK(std::get<cmcpp::StringPtr>(cmcppVals[0])->len == 32);

    //  Actual ABI Test Code  --------------------------------------------
}

TEST_CASE("component-model-cpp version")
{
    // static_assert(std::string(GREETER_VERSION) == std::string("1.0"));
    // CHECK(std::string(GREETER_VERSION) == std::string("1.0"));
}
