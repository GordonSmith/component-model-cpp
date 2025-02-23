#include "traits.hpp"
#include "lift.hpp"
#include "lower.hpp"
#include "util.hpp"
#include "host-util.hpp"

using namespace cmcpp;

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <iostream>
#include <vector>
#include <utility>
#include <cassert>
// #include <fmt/core.h>

class Heap
{
private:
    uint32_t last_alloc = 0;

public:
    std::vector<uint8_t> memory;

    Heap(size_t arg) : memory(arg), last_alloc(0)
    {
        CHECK(true);
    }

    uint32_t realloc(uint32_t original_ptr, size_t original_size, uint32_t alignment, size_t new_size)
    {
        if (original_ptr != 0 && new_size < original_size)
        {
            return align_to(original_ptr, alignment);
        }

        uint32_t ret = align_to(last_alloc, alignment);
        last_alloc = ret + new_size;
        if (last_alloc > memory.size())
        {
            std::cout << "oom: have " << memory.size() << " need " << last_alloc << std::endl;
            trap("oom");
        }
        std::memcpy(&memory[ret], &memory[original_ptr], original_size);
        return ret;
    }
};

std::unique_ptr<CallContext> createCallContext(Heap *heap, Encoding encoding)
{
    std::unique_ptr<cmcpp::InstanceContext> instanceContext = std::make_unique<cmcpp::InstanceContext>(trap, convert,
                                                                                                       [heap](int original_ptr, int original_size, int alignment, int new_size) -> int
                                                                                                       {
                                                                                                           return heap->realloc(original_ptr, original_size, alignment, new_size);
                                                                                                       });
    return instanceContext->createCallContext(heap->memory, encoding);
}

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

struct MyRecord0
{
    uint16_t age;
    uint32_t weight;
};

// TEST_CASE("Records")
// {
//     Heap heap(1024 * 1024);
//     auto cx = createCallContext(&heap, Encoding::Utf8);

//     using R0 = record_t<uint16_t, uint32_t>;
//     R0 r0 = {42, 43};
//     auto v = lower_flat(*cx, r0);
//     auto rr = lift_flat<R0>(*cx, v);
//     CHECK(r0 == rr);
//     auto rr0 = to_struct<MyRecord0>(rr);
// }
