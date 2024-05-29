#include "traits.hpp"
using namespace cmcpp;

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
    CHECK(type('0') == ValType::Char);
}

TEST_CASE("placeholders")
{

    Bool b = false;
    CHECK(type(b) == ValType::Bool);
    S8 s8 = 0;
    CHECK(type(s8) == ValType::S8);
    U8 u8 = 0;
    CHECK(type(u8) == ValType::U8);
    S16 s16 = 0;
    CHECK(type(s16) == ValType::S16);
    U16 u16 = 0;
    CHECK(type(u16) == ValType::U16);
    S32 s32 = 0;
    CHECK(type(s32) == ValType::S32);
    U32 u32 = 0;
    CHECK(type(u32) == ValType::U32);
    S64 s64 = 0;
    CHECK(type(s64) == ValType::S64);
    U64 u64 = 0;
    CHECK(type(u64) == ValType::U64);
    F32 f32 = 0;
    CHECK(type(f32) == ValType::F32);
    F64 f64 = 0;
    CHECK(type(f64) == ValType::F64);
    Char c = '0';
    CHECK(type(c) == ValType::Char);
}

TEST_CASE("traits")
{
    Bool b = false;
    bool b2 = false;
    CHECK(b == b2);
    CHECK(type(b) == type(b2));
}
