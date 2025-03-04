#include "traits.hpp"
#include "lift.hpp"
#include "lower.hpp"

#include "host-util.hpp"

using namespace cmcpp;

#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <cassert>
// #include <fmt/core.h>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

TEST_CASE("Boolean")
{
    Heap heap(1024 * 1024);
    auto cx = createCallContext(&heap, Encoding::Utf8);
    auto v = lower_flat(*cx, true);
    auto b = lift_flat<bool_t>(*cx, v);
    CHECK(b == true);
    v = lower_flat(*cx, false);
    b = lift_flat<bool_t>(*cx, v);
    CHECK(b == false);
}

const char_t chars[] = {0, 65, 0xD7FF, 0xE000, 0x10FFFF};
const char_t bad_chars[] = {0xD800, 0xDFFF, 0x110000, 0xFFFFFFFF};
TEST_CASE("Char")
{
    Heap heap(1024 * 1024);
    auto cx = createCallContext(&heap, Encoding::Utf8);
    for (auto c : chars)
    {
        auto v = lower_flat(*cx, c);
        auto c2 = lift_flat<char_t>(*cx, v);
        CHECK(c == c2);
    }
    for (auto c : bad_chars)
    {
        try
        {
            auto v = lower_flat(*cx, c);
            auto c2 = lift_flat<char_t>(*cx, v);
            CHECK(false);
        }
        catch (...)
        {
            CHECK(true);
        }
    }
}

template <typename T>
void test_numeric(const std::unique_ptr<CallContext> &cx, T v = 42)
{
    auto flat_v = lower_flat<T>(*cx, v);
    auto b = lift_flat<T>(*cx, flat_v);
    CHECK(b == v);
    v = ValTrait<T>::LOW_VALUE;
    flat_v = lower_flat<T>(*cx, v);
    b = lift_flat<T>(*cx, flat_v);
    CHECK(b == v);
    v = ValTrait<T>::HIGH_VALUE;
    flat_v = lower_flat<T>(*cx, v);
    b = lift_flat<T>(*cx, flat_v);
    CHECK(b == v);
}

TEST_CASE("Signed Integer")
{
    Heap heap(1024 * 1024);
    auto cx = createCallContext(&heap, Encoding::Utf8);
    test_numeric<int8_t>(cx);
    test_numeric<int16_t>(cx);
    test_numeric<int32_t>(cx);
    test_numeric<int64_t>(cx);
    test_numeric<int8_t>(cx, -42);
    test_numeric<int16_t>(cx, -42);
    test_numeric<int32_t>(cx, -42);
    test_numeric<int64_t>(cx, -42);
}

TEST_CASE("Unigned Integer")
{
    Heap heap(1024 * 1024);
    auto cx = createCallContext(&heap, Encoding::Utf8);
    test_numeric<uint8_t>(cx);
    test_numeric<uint16_t>(cx);
    test_numeric<uint32_t>(cx);
    test_numeric<uint64_t>(cx);
}

TEST_CASE("Float")
{
    Heap heap(1024 * 1024);
    auto cx = createCallContext(&heap, Encoding::Utf8);
    test_numeric<float32_t>(cx);
    test_numeric<float64_t>(cx);

    auto flat_v = lower_flat(*cx, std::numeric_limits<float>::infinity());
    auto b = lift_flat<float32_t>(*cx, flat_v);
    CHECK(std::isnan(b));
    flat_v = lower_flat(*cx, std::numeric_limits<double>::infinity());
    b = lift_flat<float64_t>(*cx, flat_v);
    CHECK(std::isnan(b));
}

const char *const hw = "hello World!";
const char *const hw8 = "hello 世界-🌍-!";
const char16_t *hw16 = u"hello 世界-🌍-!";

template <String T, typename T2>
void printString(T str, T2 hw)
{
    for (size_t i = 0; i < str.length(); ++i)
    {
        std::wcout << L"Character " << i << L": wstr=" << static_cast<wchar_t>(str[i]) << L", whw=" << static_cast<wchar_t>(hw[i]) << std::endl;
        if (str[i] != hw[i])
        {
            std::wcout << L"Mismatch at position " << i << std::endl;
        }
    }
}
const char *const unicode_test_strings[] = {"", "a", "\x00", "hello", "こんにちは", "你好", "안녕하세요", "Здравствуйте", "مرحبا", "שלום", "नमस्ते", "สวัสดี", "γειά σου", "hola", "bonjour", "hallo", "ciao", "Olá", "hello 世界-🌍-!", "ہیلو", "ሰላም", "ہیلو", "ਸਤ ਸ੍ਰੀ ਅਕਾਲ", "வணக்கம்", "హలో", "ಹಲೋ", "ഹലോ", "ສະບາຍດີ", "မင်္ဂလာပါ", "សួស្តី", "ສະບາຍດີ", "ສະບາຍດີ", "ສະບາຍດີ"};
const char16_t *const unicode_test_u16strings[] = {u"", u"a", u"\x00", u"hello", u"こんにちは", u"你好", u"안녕하세요", u"Здравствуйте", u"مرحبا", u"שלום", u"नमस्ते", u"สวัสดี", u"γειά σου", u"hola", u"bonjour", u"hallo", u"ciao", u"Olá", u"hello 世界-🌍-!", u"ہیلو", u"ሰላም", u"ہیلو", u"ਸਤ ਸ੍ਰੀ ਅਕਾਲ", u"வணக்கம்", u"హలో", u"ಹಲೋ", u"ഹലോ", u"ສະບາຍດີ", u"မင်္ဂလာပါ", u"សួស្តី", u"ສະບາຍດີ", u"ສະບາຍດີ", u"ສະບາຍດີ"};
const char *const boundary_test_strings[] = {
    "\x7F",             // DEL character in ASCII
    "\xC2\x80",         // First 2-byte UTF-8 character
    "\xDF\xBF",         // Last 2-byte UTF-8 character
    "\xE0\xA0\x80",     // First 3-byte UTF-8 character
    "\xEF\xBF\xBF",     // Last 3-byte UTF-8 character
    "\xF0\x90\x80\x80", // First 4-byte UTF-8 character
    "\xF4\x8F\xBF\xBF", // Last 4-byte UTF-8 character
    "\xED\x9F\xBF",     // Last valid UTF-16 character before surrogates
    "\xEE\x80\x80",     // First character after surrogates
    "\xEF\xBF\xBD",     // Replacement character (U+FFFD)
    "\xED\xA0\x7F",     // First high surrogate - 1 (Invalid UTF-8)
    "\xED\xA0\x80",     // First high surrogate (Invalid UTF-8)
    "\xED\xBF\xBF",     // Last high surrogate (Invalid UTF-8)
    "\xED\xBF\xC0",     // Last high surrogate + 1 (Invalid UTF-8)
    "\xF4\x90\x80\x80"  // Beyond U+10FFFF (Invalid UTF-8)
};
const char16_t *const boundary_test_u16strings[] = {
    u"\x7F",             // DEL character in ASCII
    u"\xC2\x80",         // First 2-byte UTF-8 character
    u"\xDF\xBF",         // Last 2-byte UTF-8 character
    u"\xE0\xA0\x80",     // First 3-byte UTF-8 character
    u"\xEF\xBF\xBF",     // Last 3-byte UTF-8 character
    u"\xF0\x90\x80\x80", // First 4-byte UTF-8 character
    u"\xF4\x8F\xBF\xBF", // Last 4-byte UTF-8 character
    u"\xED\x9F\xBF",     // Last valid UTF-16 character before surrogates
    u"\xED\xA0\x7F",     // First high surrogate - 1
    u"\xED\xA0\x80",     // First high surrogate
    u"\xED\xBF\xBF",     // Last high surrogate
    u"\xED\xBF\xC0",     // Last high surrogate + 1
    u"\xEE\x80\x80",     // First character after surrogates
    u"\xEF\xBF\xBD",     // Replacement character (U+FFFD)
    u"\xF4\x90\x80\x80"  // Invalid UTF-8 (beyond U+10FFFF)
};

TEST_CASE("String-Utf16")
{
    Heap heap(1024 * 1024);
    auto cx = createCallContext(&heap, Encoding::Utf16);
    u16string_t hw16 = u"Hello World!";
    auto v = lower_flat(*cx, hw16);
    auto hw16_ret = lift_flat<u16string_t>(*cx, v);
    CHECK(hw16_ret == hw16);

    string_t hw = "Hello World!";
    v = lower_flat(*cx, hw);
    auto hw_ret = lift_flat<string_t>(*cx, v);
    CHECK(hw_ret == hw);
}

TEST_CASE("String-Utf8")
{
    Heap heap(1024 * 1024);
    auto cx = createCallContext(&heap, Encoding::Utf8);
    string_t hw = "Hello World!";
    auto v = lower_flat(*cx, hw);
    auto hw_ret = lift_flat<string_t>(*cx, v);
    CHECK(hw_ret == hw);

    u16string_t hw16 = u"Hello World!";
    v = lower_flat(*cx, hw16);
    auto hw16_ret = lift_flat<u16string_t>(*cx, v);
    CHECK(hw16_ret == hw16);
}

TEST_CASE("String-Latin1_Utf16")
{
    Heap heap(1024 * 1024);
    auto cx = createCallContext(&heap, Encoding::Latin1_Utf16);
    string_t hw = "Hello World!";
    auto v = lower_flat(*cx, hw);
    auto hw_ret = lift_flat<string_t>(*cx, v);
    CHECK(hw_ret == hw);
    auto hw_l1_ret = lift_flat<latin1_u16string_t>(*cx, v);
    CHECK(hw_l1_ret.encoding == Encoding::Latin1);

    u16string_t hw16 = u"Hello World!";
    v = lower_flat(*cx, hw16);
    auto hw16_ret = lift_flat<u16string_t>(*cx, v);
    CHECK(hw16_ret == hw16);
    hw_l1_ret = lift_flat<latin1_u16string_t>(*cx, v);
    CHECK(hw_l1_ret.encoding == Encoding::Latin1);

    hw = "Hello 🌍!";
    v = lower_flat(*cx, hw);
    hw_ret = lift_flat<string_t>(*cx, v);
    CHECK(hw_ret == hw);
    hw_l1_ret = lift_flat<latin1_u16string_t>(*cx, v);
    CHECK(hw_l1_ret.encoding == Encoding::Utf16);

    hw16 = u"Hello 🌍!";
    v = lower_flat(*cx, hw16);
    hw16_ret = lift_flat<u16string_t>(*cx, v);
    CHECK(hw16_ret == hw16);
    hw_l1_ret = lift_flat<latin1_u16string_t>(*cx, v);
    CHECK(hw_l1_ret.encoding == Encoding::Utf16);
}

void testString(Encoding guestEncoding)
{
    Heap heap(1024 * 1024);
    auto cx = createCallContext(&heap, guestEncoding);

    for (auto hw : unicode_test_strings)
    {
        string_t hw_str = hw;
        auto v = lower_flat(*cx, hw_str);
        auto hw_str_ret = lift_flat<string_t>(*cx, v);
        // printString(hw_str_ret, hw);
        CHECK(hw_str_ret == hw_str);
    }

    for (unsigned int i = 0; i < sizeof(boundary_test_strings) / sizeof(boundary_test_strings[0]); ++i)
    {
        string_t hw_str = boundary_test_strings[i];
        auto v = lower_flat(*cx, hw_str);
        auto hw_str_ret = lift_flat<string_t>(*cx, v);
        // printString(hw_str_ret, hw);

        if (guestEncoding != Encoding::Utf8 && i >= 10)
            CHECK(hw_str_ret != hw_str);
        else
            CHECK(hw_str_ret == hw_str);
    }

    for (auto hw : unicode_test_u16strings)
    {
        u16string_t hw_str = hw;
        auto v = lower_flat(*cx, hw_str);
        auto hw_str_ret = lift_flat<u16string_t>(*cx, v);
        CHECK(hw_str_ret.length() == hw_str.length());
        // printString(hw_str_ret, hw);
        CHECK(hw_str_ret == hw_str);
    }

    for (unsigned int i = 0; i < sizeof(boundary_test_u16strings) / sizeof(boundary_test_u16strings[0]); ++i)
    {
        u16string_t hw_str = boundary_test_u16strings[i];
        auto v = lower_flat(*cx, hw_str);
        auto hw_str_ret = lift_flat<u16string_t>(*cx, v);
        // printString(hw_str_ret, hw);
        CHECK(hw_str_ret == hw_str);
    }
}

TEST_CASE("String-complex")
{
    testString(Encoding::Utf8);
    testString(Encoding::Utf16);
    testString(Encoding::Latin1_Utf16);
}

TEST_CASE("List")
{
    Heap heap(1024 * 1024);
    auto cx = createCallContext(&heap, Encoding::Utf8);

    using VariantList = list_t<variant_t<bool_t>>;
    VariantList variants = {true, false};
    auto vv5 = lower_flat(*cx, variants);
    auto v55 = lift_flat<VariantList>(*cx, vv5);
    CHECK(variants.size() == v55.size());
    auto v0_ = std::get<bool_t>(v55[0]);
    auto v1_ = std::get<bool_t>(v55[1]);
    CHECK(variants[0] == v55[0]);
    CHECK(variants[1] == v55[1]);
    CHECK(variants == v55);

    list_t<string_t> strings = {"Hello", "World", "!"};
    auto v = lower_flat(*cx, strings);
    auto strs = lift_flat<list_t<string_t>>(*cx, v);
    CHECK(strs.size() == 3);
    CHECK(strs[0] == "Hello");
    CHECK(strs[1] == "World");
    CHECK(strs[2] == "!");
}

TEST_CASE("List-Latin1_Utf16")
{
    Heap heap(1024 * 1024);
    auto cx = createCallContext(&heap, Encoding::Latin1_Utf16);
    list_t<string_t> strings = {"Hello", "World", "!"};
    auto v = lower_flat(*cx, strings);
    auto strs = lift_flat<list_t<string_t>>(*cx, v);
    CHECK(strs.size() == 3);
    CHECK(strs[0] == "Hello");
    CHECK(strs[1] == "World");
    CHECK(strs[2] == "!");
    strings = {"Hello", "🌍", "!"};
    v = lower_flat(*cx, strings);
    strs = lift_flat<list_t<string_t>>(*cx, v);
    CHECK(strs.size() == 3);
    CHECK(strs[0] == "Hello");
    CHECK(strs[1] == "🌍");
    CHECK(strs[2] == "!");
}

TEST_CASE("Flags")
{
    Heap heap(1024 * 1024);
    auto cx = createCallContext(&heap, Encoding::Latin1_Utf16);
    using MyFlags = flags_t<"a", "bb", "ccc">;
    CHECK(ValTrait<MyFlags>::size == 1);
    CHECK(MyFlags::labelsSize == 3);
    MyFlags flags(42);
    CHECK(flags.labelsSize == 3);
    auto x = flags.labels;
    CHECK(std::strcmp(x[0], "a") == 0);
    CHECK(std::strcmp(x[1], "bb") == 0);
    CHECK(std::strcmp(x[2], "ccc") == 0);
    auto v = lower_flat(*cx, flags);
    auto f = lift_flat<MyFlags>(*cx, v);
    CHECK(flags.test<"a">() == f.test<"a">());
    CHECK(flags.test<"bb">() == f.test<"bb">());
    CHECK(flags.test<"ccc">() == f.test<"ccc">());
    CHECK(flags == f);

    using MyFlags2 = flags_t<"one", "two", "three", "four", "five", "six", "seven", "8", "nine">;
    CHECK(ValTrait<MyFlags2>::size == 2);
    CHECK(MyFlags2::labelsSize == 9);
    MyFlags2 flags2;
    flags2.set<"nine">();
    auto vi = lower_flat(*cx, flags2);
    auto f2 = lift_flat<MyFlags2>(*cx, vi);
    CHECK(flags2 == f2);
    CHECK(flags2.test<"one">() == false);
    CHECK(flags2.test<"8">() == false);
    CHECK(flags2.test<"nine">() == true);
    flags2.reset<"nine">();
    CHECK(flags2.test<"nine">() == false);
    flags2.set<"nine">();
    CHECK(flags2.test<"nine">() == true);

    CHECK(f2.test<"one">() == false);
    CHECK(f2.test<"8">() == false);
    CHECK(f2.test<"nine">() == true);
    f2.reset<"nine">();
    CHECK(f2.test<"nine">() == false);
    f2.set<"nine">();
    CHECK(f2.test<"nine">() == true);
}

TEST_CASE("Tuples")
{
    Heap heap(1024 * 1024);
    auto cx = createCallContext(&heap, Encoding::Utf8);

    using R0 = tuple_t<uint16_t, uint32_t>;
    auto flatTypes = ValTrait<R0>::flat_types;
    CHECK(flatTypes.size() == 2);
    R0 r0 = {42, 43};
    auto v = lower_flat(*cx, r0);
    auto rr = lift_flat<R0>(*cx, v);
    CHECK(r0 == rr);
    // auto str0 = to_struct<MyRecord0>(rr);

    using R1 = tuple_t<uint16_t, uint32_t, string_t>;
    R1 r1 = {142, 143, "Hello"};
    auto vv = lower_flat(*cx, r1);
    auto rr1 = lift_flat<R1>(*cx, vv);
    CHECK(r1 == rr1);
    // auto str1 = to_struct<MyRecord1>(rr1);

    using R2 = tuple_t<uint16_t, uint32_t, string_t, list_t<string_t>>;
    R2 r2 = {242, 243, "2Hello", {"2World", "!"}};
    auto vvv = lower_flat(*cx, r2);
    auto rr2 = lift_flat<R2>(*cx, vvv);
    CHECK(r2 == rr2);
    // auto str2 = to_struct<MyRecord2>(rr2);
}

TEST_CASE("Records")
{
    Heap heap(1024 * 1024);
    auto cx = createCallContext(&heap, Encoding::Utf8);

    struct PersonStruct
    {
        string_t name;
        uint16_t age;
        uint32_t weight;
    };
    using Person = record_t<PersonStruct>;
    Person p_in = {"John", 42, 200};
    auto v = lower_flat(*cx, p_in);
    auto p_out = lift_flat<Person>(*cx, v);
    CHECK(p_in.name == p_out.name);
    CHECK(p_in.age == p_out.age);
    CHECK(p_in.weight == p_out.weight);

    struct PersonExStruct
    {
        string_t name;
        uint16_t age;
        uint32_t weight;
        string_t address;
    };
    using PersonEx = record_t<PersonExStruct>;
    PersonEx pex_in = {"John", 42, 200, "123 Main St."};
    v = lower_flat(*cx, pex_in);
    auto pex_out = lift_flat<PersonEx>(*cx, v);
    CHECK(pex_in.name == pex_out.name);
    CHECK(pex_in.age == pex_out.age);
    CHECK(pex_in.weight == pex_out.weight);
    CHECK(pex_in.address == pex_out.address);

    struct PersonEx2Struct
    {
        string_t name;
        uint16_t age;
        uint32_t weight;
        string_t address;
        list_t<string_t> phones;
    };
    using PersonEx2 = record_t<PersonEx2Struct>;
    PersonEx2 pex2_in = {"John", 42, 200, "123 Main St.", {"555-1212", "555-1234"}};
    v = lower_flat(*cx, pex2_in);
    auto pex2_out = lift_flat<PersonEx2>(*cx, v);
    CHECK(pex2_in.name == pex2_out.name);
    CHECK(pex2_in.age == pex2_out.age);
    CHECK(pex2_in.weight == pex2_out.weight);
    CHECK(pex2_in.address == pex2_out.address);
    CHECK(pex2_in.phones == pex2_out.phones);

    struct PersonEx3Struct
    {
        string_t name;
        uint16_t age;
        uint32_t weight;
        string_t address;
        list_t<string_t> phones;
        tuple_t<uint16_t, uint32_t> vital_stats;
        record_t<PersonEx2Struct> p;
    };
    using PersonEx3 = record_t<PersonEx3Struct>;
    PersonEx3 pex3_in = {"John", 42, 200, "123 Main St.", {"555-1212", "555-1234"}, {42, 43}, {"Jane", 43, 150, "63 Elm St.", {"555-1212", "555-1234", "555-4321"}}};
    v = lower_flat(*cx, pex3_in);
    auto pex3_out = lift_flat<PersonEx3>(*cx, v);
    CHECK(pex3_in.name == pex3_out.name);
    CHECK(pex3_in.age == pex3_out.age);
    CHECK(pex3_in.weight == pex3_out.weight);
    CHECK(pex3_in.address == pex3_out.address);
    CHECK(pex3_in.phones == pex3_out.phones);
    CHECK(pex3_in.vital_stats == pex3_out.vital_stats);
    CHECK(pex3_in.p.name == pex3_out.p.name);
    CHECK(pex3_in.p.age == pex3_out.p.age);
    CHECK(pex3_in.p.weight == pex3_out.p.weight);
    CHECK(pex3_in.p.address == pex3_out.p.address);
    CHECK(pex3_in.p.phones == pex3_out.p.phones);
}

TEST_CASE("Variant")
{
    Heap heap(1024 * 1024);
    auto cx = createCallContext(&heap, Encoding::Utf8);

    using V0 = variant_t<uint16_t, uint32_t>;
    auto x = ValTrait<ValTrait<V0>::discriminant_type>::size;
    V0 v0 = static_cast<uint32_t>(42);
    auto vv0 = lower_flat(*cx, v0);
    auto v00 = lift_flat<V0>(*cx, vv0);
    CHECK(v0 == v00);

    using V1 = variant_t<uint16_t, uint32_t, string_t>;
    auto x1 = ValTrait<ValTrait<V1>::discriminant_type>::size;
    V1 v1 = "Hello";
    auto vv1 = lower_flat(*cx, v1);
    auto v11 = lift_flat<V1>(*cx, vv1);
    CHECK(v1 == v11);

    using V2 = variant_t<uint16_t, uint32_t, string_t, list_t<string_t>>;
    auto x2 = ValTrait<ValTrait<V2>::discriminant_type>::size;
    V2 v2 = list_t<string_t>{"Hello", "World", "!"};
    auto vv2 = lower_flat(*cx, v2);
    auto v22 = lift_flat<V2>(*cx, vv2);
    CHECK(v2 == v22);

    using V3 = variant_t<uint16_t, uint32_t, string_t, list_t<string_t>, tuple_t<uint16_t, uint32_t>>;
    auto x3 = ValTrait<ValTrait<V3>::discriminant_type>::size;
    V3 v3 = tuple_t<uint16_t, uint32_t>{42, 43};
    auto vv3 = lower_flat(*cx, v3);
    auto v33 = lift_flat<V3>(*cx, vv3);
    CHECK(v3 == v33);

    using V4 = variant_t<uint16_t, uint32_t, string_t, list_t<string_t>, tuple_t<uint16_t, uint32_t>, V3>;
    auto x4 = ValTrait<ValTrait<V4>::discriminant_type>::size;
    V4 v4 = tuple_t<uint16_t, uint32_t>{42, 43};
    auto vv4 = lower_flat(*cx, v4);
    auto v44 = lift_flat<V4>(*cx, vv4);
    auto rr4 = std::get<tuple_t<uint16_t, uint32_t>>(v44);
    CHECK(v4 == v44);

    using VariantList = list_t<variant_t<bool_t>>;
    VariantList variants = {true, false};
    auto vv5 = lower_flat(*cx, variants);
    auto v55 = lift_flat<VariantList>(*cx, vv5);
    CHECK(variants.size() == v55.size());
    auto v0_ = std::get<bool_t>(v55[0]);
    auto v1_ = std::get<bool_t>(v55[1]);
    CHECK(variants[0] == v55[0]);
    CHECK(variants[1] == v55[1]);
    CHECK(variants == v55);

    using VariantList2 = list_t<variant_t<string_t>>;
    VariantList2 variants2 = {"Hello World1"};
    auto vv6 = lower_flat(*cx, variants2);
    auto v66 = lift_flat<VariantList2>(*cx, vv6);
    CHECK(variants2 == v66);
}
