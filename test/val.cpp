#include "val.hpp"
using namespace cmcpp;

#include <fmt/core.h>
#include <doctest/doctest.h>

TEST_CASE("primatives")
{
    CHECK(type(false) == ValType::Bool);
    CHECK(type((int8_t)0) == ValType::S8);
    CHECK(type((uint8_t)0) == ValType::U8);
    CHECK(type((int16_t)0) == ValType::S16);
    CHECK(type((uint16_t)0) == ValType::U16);
    CHECK(type((int32_t)0) == ValType::S32);
    CHECK(type((uint32_t)0) == ValType::U32);
    CHECK(type((int64_t)0) == ValType::S64);
    CHECK(type((uint64_t)0) == ValType::U64);
    CHECK(type((float32_t)0) == ValType::F32);
    CHECK(type((float64_t)0) == ValType::F64);
    CHECK(type(L'0') == ValType::Char);
}

TEST_CASE("Val")
{
    Val b = false;
    CHECK(valType(b) == ValType::Bool);
    CHECK(std::get<bool>(b) == false);

    Val u8 = (uint8_t)0;
    CHECK(valType(u8) == ValType::U8);
    CHECK(std::get<uint8_t>(u8) == 0);

    Val s16 = (int16_t)0;
    CHECK(valType(s16) == ValType::S16);
    CHECK(std::get<int16_t>(s16) == 0);

    Val u16 = (uint16_t)0;
    CHECK(valType(u16) == ValType::U16);
    CHECK(std::get<uint16_t>(u16) == 0);

    Val s32 = (int32_t)0;
    CHECK(valType(s32) == ValType::S32);
    CHECK(std::get<int32_t>(s32) == 0);

    Val u32 = (uint32_t)0;
    CHECK(valType(u32) == ValType::U32);
    CHECK(std::get<uint32_t>(u32) == 0);

    Val s64 = (int64_t)0;
    CHECK(valType(s64) == ValType::S64);
    CHECK(std::get<int64_t>(s64) == 0);

    Val u64 = (uint64_t)0;
    CHECK(valType(u64) == ValType::U64);
    CHECK(std::get<uint64_t>(u64) == 0);

    Val f32 = (float32_t)0;
    CHECK(valType(f32) == ValType::F32);
    CHECK(std::get<float32_t>(f32) == 0);

    Val f64 = (float64_t)0;
    CHECK(valType(f64) == ValType::F64);
    CHECK(std::get<float64_t>(f64) == 0);

    Val c = L'0';
    CHECK(valType(c) == ValType::Char);
    CHECK(std::get<wchar_t>(c) == '0');
}

TEST_CASE("WasmVal")
{
    WasmVal s32 = (int32_t)0;
    CHECK(wasmValType(s32) == ValType::S32);
    CHECK(std::get<int32_t>(s32) == 0);

    WasmVal s64 = (int64_t)0;
    CHECK(wasmValType(s64) == ValType::S64);
    CHECK(std::get<int64_t>(s64) == 0);

    WasmVal f32 = (float32_t)0;
    CHECK(wasmValType(f32) == ValType::F32);
    CHECK(std::get<float32_t>(f32) == 0);

    WasmVal f64 = (float64_t)0;
    CHECK(wasmValType(f64) == ValType::F64);
    CHECK(std::get<float64_t>(f64) == 0);
}

TEST_CASE("other")
{
    std::vector<std::pair<const WasmVal &, const Val &>> pairs;
    WasmVal x = 0;
    Val y = false;
    fmt::print("first: {}\n", std::get<int32_t>(x));
    pairs.push_back({x, y});
    for (auto pairX : pairs)
    {
        auto first = pairX.first;
        auto second = pairX.second;
        // fmt::print("first: {}\n", first);
        // fmt::print("second: {}\n", second);
    }
}