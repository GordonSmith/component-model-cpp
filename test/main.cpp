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

// void test(const Val &t, std::vector<WasmVal> vals_to_lift, std::optional<Val> v,
//           const CallContextPtr &cx = mk_cx(),
//           std::optional<Encoding> dst_encoding = std::nullopt,
//           std::optional<Val> lower_t = std::nullopt,
//           std::optional<Val> lower_v = std::nullopt)
// {
//     auto vi = CoreValueIter(vals_to_lift);
//     if (!v.has_value())
//     {
//         CHECK_THROWS(lift_flat(*cx, vi, t));
//     }

//     Val got = lift_flat(*cx, vi, t);
//     printV("got:  ", got);
//     const char *got_type = valTypeName(valType(got));
//     const char *expected_type = valTypeName(valType(v.value()));

//     CHECK(vi.i == vi.values.size());
//     CHECK(valType(got) == valType(v.value()));

//     // fmt.print("got: {},  v: {}", valTypeName(valType(got)), valTypeName(valType(v.value())));
//     auto got32 = valType(got) == ValType::U32 ? std::get<uint32_t>(got) : 0;
//     auto expected32 = valType(got) == ValType::U32 ? std::get<uint32_t>(v.value()) : 0;
//     auto gotChar = valType(got) == ValType::Char ? std::get<wchar_t>(got) : 0;
//     auto expectedChar = valType(got) == ValType::Char ? std::get<wchar_t>(v.value()) : 0;
//     auto gotString = valType(got) == ValType::String ? std::get<string_ptr>(got)->to_string() : "";
//     auto expectedV = valType(got) == ValType::String ? std::get<string_ptr>(v.value()) : string_ptr();
//     auto expectedString = valType(got) == ValType::String ? std::get<string_ptr>(v.value())->to_string() : "";

//     CHECK(got == v.value());
//     if (!lower_t.has_value())
//     {
//         lower_t = t;
//         CHECK(lower_t.value() == t);
//     }
//     if (!lower_v.has_value())
//     {
//         lower_v = v;
//         CHECK(lower_v.value() == v.value());
//     }

//     Heap heap(5 * cx.opts->memory.size());
//     if (!dst_encoding.has_value())
//     {
//         dst_encoding = cx.opts->string_encoding;
//     }
//     auto cx2 = mk_cx(heap.memory, dst_encoding.value(), [&heap](int original_ptr, int original_size, int alignment, int new_size) -> int
//                      { return heap.realloc(original_ptr, original_size, alignment, new_size); });
//     auto lowered_vals = lower_flat(*cx2, v.value(), lower_t.value());

//     auto vi2 = CoreValueIter(lowered_vals);
//     got = lift_flat(*cx2, vi2, lower_t.value());
//     CHECK(valType(got) == valType(lower_v.value()));
//     // CHECK(!equal_modulo_string_encoding(got, lower_v));
// }

TEST_CASE("Boolean")
{
    Heap heap(1024 * 1024);
    auto cx = createCallContext(&heap, Encoding::Utf8);
    auto v = lower_flat<bool_t>(*cx, true);
    auto b = lift_flat<bool_t>(*cx, v);
    CHECK(b == true);
    v = lower_flat<bool_t>(*cx, false);
    b = lift_flat<bool_t>(*cx, v);
    CHECK(b == false);
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
}

const char *const hw = "hello world";
TEST_CASE("String")
{
    Heap heap(1024 * 1024);
    auto cx = createCallContext(&heap, Encoding::Utf8);
    auto v = lower_flat(*cx, string_t{Encoding::Utf8, (const char8_t *)hw, strlen(hw)});
    auto str = lift_flat<string_t>(*cx, v);
    CHECK(str.encoding == Encoding::Utf8);
    CHECK(str.byte_len == strlen(hw));
    CHECK(std::string((const char *)str.ptr, str.byte_len) == hw);
}

TEST_CASE("List")
{
    Heap heap(1024 * 1024);
    auto cx = createCallContext(&heap, Encoding::Utf8);
    list_t<string_t> strings = {string_t{Encoding::Utf8, (const char8_t *)hw, 5}, string_t{Encoding::Utf8, (const char8_t *)hw, 3}};
    auto v = lower_flat(*cx, strings);
    auto strs = lift_flat<list_t<string_t>>(*cx, v);
    CHECK(strs.size() == 2);
    CHECK(strs[0].encoding == Encoding::Utf8);
    CHECK(strs[0].byte_len == 5);
    CHECK(std::string((const char *)strs[0].ptr, strs[0].byte_len) == std::string(hw, strs[0].byte_len));
    CHECK(strs[1].encoding == Encoding::Utf8);
    CHECK(strs[1].byte_len == 3);
    CHECK(std::string((const char *)strs[1].ptr, strs[1].byte_len) == std::string(hw, strs[1].byte_len));
    v = lower_flat(*cx, strings);
    strs = lift_flat<list_t<string_t>>(*cx, v);
    CHECK(strs.size() == 2);
    CHECK(strs[0].encoding == Encoding::Utf8);
    CHECK(strs[0].byte_len == 5);
    CHECK(std::string((const char *)strs[0].ptr, strs[0].byte_len) == std::string(hw, strs[0].byte_len));
    CHECK(strs[1].encoding == Encoding::Utf8);
    CHECK(strs[1].byte_len == 3);
    CHECK(std::string((const char *)strs[1].ptr, strs[1].byte_len) == std::string(hw, strs[1].byte_len));
}

TEST_CASE("List2")
{
    Heap heap(1024 * 1024);
    auto cx = createCallContext(&heap, Encoding::Utf8);
    list_t<list_t<string_t>> strings = {{string_t{Encoding::Utf8, (const char8_t *)hw, 5}, string_t{Encoding::Utf8, (const char8_t *)hw, 3}}, {string_t{Encoding::Utf8, (const char8_t *)hw, 5}, string_t{Encoding::Utf8, (const char8_t *)hw, 3}}, {string_t{Encoding::Utf8, (const char8_t *)hw, 5}, string_t{Encoding::Utf8, (const char8_t *)hw, 3}}};
    auto v = lower_flat(*cx, strings);
    auto strs = lift_flat<list_t<list_t<string_t>>>(*cx, v);
    CHECK(strs.size() == 3);
    CHECK(strs[0][0].encoding == Encoding::Utf8);
    CHECK(strs[0][0].byte_len == 5);
    CHECK(std::string((const char *)strs[0][0].ptr, strs[0][0].byte_len) == std::string(hw, strs[0][0].byte_len));
    CHECK(strs[1][0].encoding == Encoding::Utf8);
    CHECK(strs[1][0].byte_len == 5);
    CHECK(std::string((const char *)strs[0][1].ptr, strs[0][1].byte_len) == std::string(hw, strs[0][1].byte_len));
    v = lower_flat(*cx, strings);
    strs = lift_flat<list_t<list_t<string_t>>>(*cx, v);
    CHECK(strs.size() == 3);
    CHECK(strs[0][0].encoding == Encoding::Utf8);
    CHECK(strs[0][0].byte_len == 5);
    CHECK(std::string((const char *)strs[0][0].ptr, strs[0][0].byte_len) == std::string(hw, strs[0][0].byte_len));
    CHECK(strs[1][0].encoding == Encoding::Utf8);
    CHECK(strs[1][0].byte_len == 5);
    CHECK(std::string((const char *)strs[0][1].ptr, strs[0][1].byte_len) == std::string(hw, strs[0][1].byte_len));
}

struct MyRecord0
{
    uint16_t age;
    uint32_t weight;
};

TEST_CASE("Records")
{
    Heap heap(1024 * 1024);
    auto cx = createCallContext(&heap, Encoding::Utf8);

    using R0 = record_t<uint16_t, uint32_t>;
    R0 r0 = {42, 43};
    auto v = lower_flat(*cx, r0);
    auto rr = lift_flat<R0>(*cx, v);
    CHECK(r0 == rr);
    auto rr0 = to_struct<MyRecord0>(rr);
    // CHECK(r2.age == rr0.age);
    // CHECK(r2.weight == rr0.weight);

    // StructWithFields<string_t, uint16_t> r2 = {string_t{Encoding::Utf8, (const char8_t *)"name", 5}, {42}};

    // auto v = lower_flat<record_t<MyRecord1>>(*cx, r0);
    // auto rr = lift_flat<MyRecord1>(*cx, v);
    // CHECK(std::get<0>(rr).label == "name");
    // CHECK(std::get<0>(rr).v.encoding == Encoding::Utf8);
    // CHECK(std::get<0>(rr).v.byte_len == 5);
    // CHECK(std::get<1>(rr).label == "age");
    // CHECK(std::get<1>(rr).v == 42);
    // record_t<field_t<"ages", list_t<uint32_t>>> ages = {{list_t<uint32_t>{42, 43, 44}}};
    // auto v2 = lower_flat(*cx, ages);
    // auto rr2 = lift_flat<decltype(ages)>(*cx, v2);
    // CHECK(std::get<0>(rr2).label == "ages");
    // CHECK(std::get<0>(rr2).v[0] == 42);
    // CHECK(std::get<0>(rr2).v[1] == 43);
    // CHECK(std::get<0>(rr2).v[2] == 44);
    // record_t<field_t<"ages", list_t<uint32_t>>> ages_record = record_t{list_t<uint32_t>{42, 43, 44}};
    // auto v2 = lower_flat<record_t<field_t<"ages", list_t<uint32_t>>>>(*cx, ages_record);

    // record_t<string_t, uint32_t> r = { {"name", {Encoding::Utf8, (const char8_t *)hw, 5}}, {"age", 42} };
    // auto v = lower_flat<record_t<string_t, uint32_t>>(*cx, r);
    // auto rr = lift_flat<record_t<string_t, uint32_t>>(*cx, v);
    // CHECK(std::string((const char *)std::get<0>(rr).v.ptr, std::get<0>(rr).v.byte_len) == std::string(hw, std::get<0>(rr).v.byte_len));
    // CHECK(std::get<1>(rr).label == "age");
    // CHECK(std::get<1>(rr).v == 42);
    // record_t<string_t, uint32_t> r2 = { {"name", {Encoding::Utf8, (const char8_t *)hw, 5}}, {"age", 42} };

    // CHECK(rr.fields[0].v.byte_len == 5);
    // CHECK(std::string((const char *)rr.fields[0].v.ptr, rr.fields[0].v.byte_len) == std::string(hw, rr.fields[0].v.byte_len));
    // CHECK(rr.fields[1].v == 42);
    // record_t<int32_t, string_t> r2 = {string_t{Encoding::Utf8, (const char8_t *)hw, 5}, 42};
    // v = lower_flat(*cx, r2);
    // rr = lift_flat<record_t<int32_t, string_t>>(*cx, v);
    // CHECK(rr.fields[0].v.encoding == Encoding::Utf8);
    // CHECK(rr.fields[0].v.byte_len == 5);
    // CHECK(std::string((const char *)rr.fields[0].v.ptr, rr.fields[0].v.byte_len) == std::string(hw, rr.fields[0].v.byte_len));
    // CHECK(rr.fields[1].v == 42);
}

// TEST_CASE("Tuple")
// {
//     auto tt = std::make_shared<tuple_t>(std::vector<Val>{std::make_shared<tuple_t>(std::vector<Val>{uint8_t(), uint8_t()}),
//                                                          uint8_t()});

//     auto r = std::make_shared<record_t>(std::vector<field_ptr>{std::make_shared<field_t>("0", std::make_shared<record_t>(std::vector<field_ptr>{
//                                                                                                   std::make_shared<field_t>("0", (uint8_t)1),
//                                                                                                   std::make_shared<field_t>("1", (uint8_t)2)})),
//                                                                std::make_shared<field_t>("1", (uint8_t)3)});
//     test(tt, {1, 2, 3}, r);
// }

// TEST_CASE("Flags")
// {
//     auto f = std::make_shared<flags_t>(std::vector<std::string>{"a", "b"});
//     test(f, {0}, std::make_shared<flags_t>(std::vector<std::string>{}));
//     test(f, {1}, std::make_shared<flags_t>(std::vector<std::string>{"a"}));
//     test(f, {2}, std::make_shared<flags_t>(std::vector<std::string>{"b"}));
//     test(f, {3}, std::make_shared<flags_t>(std::vector<std::string>{"a", "b"}));
//     test(f, {4}, std::make_shared<flags_t>(std::vector<std::string>{}));

//     std::vector<std::string> strVec(33);
//     std::vector<std::string> strVec2(33);
//     for (int i = 0; i < 33; i++)
//     {
//         strVec[i] = std::to_string(i);
//         strVec2[i] = std::to_string(i);
//     }
//     test(std::make_shared<flags_t>(strVec), {(int32_t)0xffffffff, (int32_t)0x1}, std::make_shared<flags_t>(strVec2));
// }

// TEST_CASE("Option")
// {
//     auto t = std::make_shared<option_t>(float32_t());
//     test(t, {0, (float32_t)3.14}, std::make_shared<variant_t>(std::vector<case_ptr>{std::make_shared<case_t>("None")}));
//     test(t, {1, (float32_t)3.14}, std::make_shared<variant_t>(std::vector<case_ptr>{std::make_shared<case_t>("Some", (float32_t)3.14)}));
// }

// TEST_CASE("Result")
// {
//     auto t = std::make_shared<result_t>(uint8_t(), uint32_t());
//     test(t, {0, 42}, std::make_shared<variant_t>(std::vector<case_ptr>{std::make_shared<case_t>("Ok", (uint8_t)42)}));
//     test(t, {1, 1000}, std::make_shared<variant_t>(std::vector<case_ptr>{std::make_shared<case_t>("Error", (uint32_t)1000)}));
// }

// using WasmValValPair = std::pair<WasmVal, std::optional<Val>>;
// void test_pairs(Val t, std::vector<WasmValValPair> pairs)
// {
//     for (auto pair : pairs)
//     {
//         const char *wasm_type = valTypeName(wasmValType(pair.first));
//         const char *expected_type = pair.second.has_value() ? valTypeName(valType(pair.second.value())) : "";
//         uint32_t wasm32 = wasmValType(pair.first) == ValType::S32 ? std::get<int32_t>(pair.first) : 0;
//         uint32_t val32 = pair.second.has_value() ? valType(pair.second.value()) == ValType::U32 ? std::get<uint32_t>(pair.second.value()) : 0 : 0;
//         uint64_t wasm64 = wasmValType(pair.first) == ValType::S64 ? std::get<int64_t>(pair.first) : 0;
//         uint64_t val64 = pair.second.has_value() ? valType(pair.second.value()) == ValType::U64 ? std::get<uint64_t>(pair.second.value()) : 0 : 0;
//         int64_t valS64 = pair.second.has_value() ? valType(pair.second.value()) == ValType::S64 ? std::get<int64_t>(pair.second.value()) : 0 : 0;
//         test(t, {pair.first}, pair.second);
//     }
// }

// template <typename T>
// void testValPair(const std::pair<const Val &, T> &pair)
// {
//     CHECK(std::get<T>(pair.first) == pair.second);
// }

// template <typename T>
// void testValVector(const std::vector<std::pair<const Val &, T>> &pairs)
// {
//     for (auto pair : pairs)
//     {
//         testValPair<T>(pair);
//     }
// }

// void testWasmValVal(const std::vector<WasmValValPair> &pairs)
// {
//     for (const WasmValValPair &pair : pairs)
//     {
//         const char *wasm_type = valTypeName(wasmValType(pair.first));
//         const char *expected_type = valTypeName(valType(pair.second.value()));
//         CHECK(true);
//     }
// }

// template <typename T>
// void testVal(const Val &v, T expected)
// {
//     CHECK(std::get<T>(v) == expected);
//     testValPair<T>({v, expected});
//     testValVector<T>({{v, expected}});
//     testWasmValVal({{expected, v}});
// }

// void test_string_internal(Encoding src_encoding, Encoding dst_encoding, const std::string &s, const string_ptr &encoded, size_t tagged_code_units)
// {
//     Heap heap(encoded->byte_len());
//     std::memcpy(heap.memory.data(), encoded->ptr(), encoded->byte_len());
//     CallContextPtr cx = mk_cx(heap.memory, src_encoding);

//     char8_t *v_mem = (char8_t *)std::malloc(s.length() * 4);
//     auto _v = convert(v_mem, (const char8_t *)s.c_str(), s.length(), Encoding::Utf8, src_encoding);
//     string_ptr v = std::make_shared<string_t>((const char *)_v.first, src_encoding, _v.second);
//     test(string_ptr(), {0, (int32_t)tagged_code_units}, v, cx, dst_encoding);
// }

// void test_string(Encoding src_encoding, Encoding dst_encoding, const std::string &s)
// {
//     size_t worst_case_size = s.length() * 4;
//     size_t tagged_code_units = 0;

//     switch (src_encoding)
//     {
//     case Encoding::Latin1:
//     case Encoding::Utf8:
//     {
//         char8_t *encoded_mem = (char8_t *)std::malloc(worst_case_size);
//         auto _encoded = convert(encoded_mem, (const char8_t *)s.c_str(), s.length(), Encoding::Utf8, src_encoding);
//         auto encoded = std::make_shared<string_t>((const char *)_encoded.first, src_encoding, _encoded.second);
//         tagged_code_units = _encoded.second;
//         test_string_internal(src_encoding, dst_encoding, s, encoded, tagged_code_units);
//         break;
//     }
//     case Encoding::Utf16:
//     {
//         char8_t *encoded_mem = (char8_t *)std::malloc(worst_case_size);
//         auto _encoded = convert(encoded_mem, (const char8_t *)s.c_str(), s.length(), Encoding::Utf8, src_encoding);
//         auto encoded = std::make_shared<string_t>((const char *)_encoded.first, src_encoding, _encoded.second);
//         tagged_code_units = _encoded.second / 2;
//         test_string_internal(src_encoding, dst_encoding, s, encoded, tagged_code_units);
//         break;
//     }
//     case Encoding::Latin1_Utf16:
//         try
//         {
//             char8_t *encoded_mem = (char8_t *)std::malloc(worst_case_size);
//             auto _encoded = convert(encoded_mem, (const char8_t *)s.c_str(), s.length(), Encoding::Utf8, Encoding::Latin1);
//             auto encoded = std::make_shared<string_t>((const char *)_encoded.first, src_encoding, _encoded.second);
//             tagged_code_units = _encoded.second;
//             test_string_internal(src_encoding, dst_encoding, s, encoded, tagged_code_units);
//             break;
//         }
//         catch (...)
//         {
//             char8_t *encoded_mem = (char8_t *)std::malloc(worst_case_size);
//             auto _encoded = convert(encoded_mem, (const char8_t *)s.c_str(), s.length(), Encoding::Utf8, Encoding::Utf16);
//             auto encoded = std::make_shared<string_t>((const char *)_encoded.first, Encoding::Latin1, _encoded.second);
//             tagged_code_units = _encoded.second / 2 | UTF16_TAG;
//             test_string_internal(src_encoding, dst_encoding, s, encoded, tagged_code_units);
//             break;
//         }
//     }
// }

// TEST_CASE("String")
// {
//     std::vector<Encoding> encodings = {Encoding::Utf8, Encoding::Utf16}; //, Encoding::Latin1_Utf16};
//     std::vector<std::string> fun_strings = {
//         "",
//         "a",
//         "hi",
//         "\x00",
//         "a\x00b",
//         "\x7f",
//         "\u007f",
//         "\xc2\x80",
//         "\u0080",
//         "Hello, world!",
//         "こんにちは世界！",
//         "Hola, mundo!",
//         "Bonjour, le monde!",
//         "Hallo, Welt!",
//         "Привет, мир!",
//         "\u0080b",
//         // "ab\xefc",
//         "\u00ab\u00ef\u000c",
//         "\u01ffy",
//         "xy\u01ff",
//         "a\ud7ffb",
//         "a\u02ff\u03ff\u04ffbc",
//         "\uf123",
//         "\uf123\uf123abc",
//         "abcdef\uf123",
//     };
//     for (auto src_encoding : encodings)
//     {
//         for (auto dst_encoding : encodings)
//         {
//             for (auto s : fun_strings)
//             {
//                 test_string(src_encoding, dst_encoding, s);
//             }
//         }
//     }
// }
// // void test_heap(Val t, Val expected, std::vector<WasmVal> vals_to_lift, const std::vector<uint8_t> byte_array)
// // {
// //     Heap heap(byte_array.size());
// //     memcpy(heap.memory.data(), byte_array.data(), byte_array.size());

// //     auto cx = mk_cx(heap.memory);
// //     test(t, vals_to_lift, expected, std::nullopt, std::nullopt, cx);
// // }

// // TEST_CASE("List")
// // {
// //     test_heap(std::make_shared<list_t>(bool()), std::make_shared<list_t>(bool(), std::vector<Val>{true, false, true}), {0, 3}, {1, 0, 1});
// //     test_heap(std::make_shared<list_t>(bool()), std::make_shared<list_t>(bool(), std::vector<Val>{true, false, true}), {0, 3}, {1, 0, 2});
// //     test_heap(std::make_shared<list_t>(bool()), std::make_shared<list_t>(bool(), std::vector<Val>{true, false, true}), {3, 3}, {0xff, 0xff, 0xff, 1, 0, 1});
// //     test_heap(std::make_shared<list_t>(uint8_t()), std::make_shared<list_t>(uint8_t(), std::vector<Val>{(uint8_t)1, (uint8_t)2, (uint8_t)3}), {0, 3}, {1, 2, 3});
// //     test_heap(std::make_shared<list_t>(uint16_t()), std::make_shared<list_t>(uint16_t(), std::vector<Val>{(uint16_t)1, (uint16_t)2, (uint16_t)3}), {0, 3}, {1, 0, 2, 0, 3, 0});
// //     test_heap(std::make_shared<list_t>(uint32_t()), std::make_shared<list_t>(uint32_t(), std::vector<Val>{(uint32_t)1, (uint32_t)2, (uint32_t)3}), {0, 3}, {1, 0, 0, 0, 2, 0, 0, 0, 3, 0, 0, 0});
// //     test_heap(std::make_shared<list_t>(uint64_t()), std::make_shared<list_t>(uint64_t(), std::vector<Val>{(uint64_t)1, (uint64_t)2}), {0, 2}, {1, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0});
// //     test_heap(std::make_shared<list_t>(int8_t()), std::make_shared<list_t>(int8_t(), std::vector<Val>{(int8_t)-1, (int8_t)-2, (int8_t)-3}), {0, 3}, {0xff, 0xfe, 0xfd});
// //     test_heap(std::make_shared<list_t>(int16_t()), std::make_shared<list_t>(int16_t(), std::vector<Val>{(int16_t)-1, (int16_t)-2, (int16_t)-3}), {0, 3}, {0xff, 0xff, 0xfe, 0xff, 0xfd, 0xff});
// //     test_heap(std::make_shared<list_t>(int32_t()), std::make_shared<list_t>(int32_t(), std::vector<Val>{(int32_t)-1, (int32_t)-2, (int32_t)-3}), {0, 3}, {0xff, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xfd, 0xff, 0xff, 0xff});
// //     test_heap(std::make_shared<list_t>(int64_t()), std::make_shared<list_t>(int64_t(), std::vector<Val>{(int64_t)-1, (int64_t)-2}), {0, 2}, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});
// //     test_heap(std::make_shared<list_t>(wchar_t()), std::make_shared<list_t>(wchar_t(), std::vector<Val>{L'A', L'B', L'c'}), {0, 3}, {65, 0, 0, 0, 66, 0, 0, 0, 99, 0, 0, 0});
// //     test_heap(std::make_shared<list_t>(string_ptr()), std::make_shared<list_t>(string_ptr(), std::vector<Val>{std::make_shared<string_t>("hi"), std::make_shared<string_t>("wat")}), {0, 2}, {16, 0, 0, 0, 2, 0, 0, 0, 21, 0, 0, 0, 3, 0, 0, 0, 'h', 'i', 0xf, 0xf, 0xf, 'w', 'a', 't'});
// //     // test_heap(List(List(U8())), [[3,4,5],[],[6,7]], [0,3], [24,0,0,0, 3,0,0,0, 0,0,0,0, 0,0,0,0, 27,0,0,0, 2,0,0,0, 3,4,5,  6,7])
// //     // test_heap(List(List(U16())), [[5,6]], [0,1], [8,0,0,0, 2,0,0,0, 5,0, 6,0])
// //     // test_heap(List(List(U16())), None, [0,1], [9,0,0,0, 2,0,0,0, 0, 5,0, 6,0])
// //     // test_heap(List(Tuple([U8(),U8(),U16(),U32()])), [mk_tup(6,7,8,9),mk_tup(4,5,6,7)], [0,2], [6, 7, 8,0, 9,0,0,0,   4, 5, 6,0, 7,0,0,0])
// //     // test_heap(List(Tuple([U8(),U16(),U8(),U32()])), [mk_tup(6,7,8,9),mk_tup(4,5,6,7)], [0,2], [6,0xff, 7,0, 8,0xff,0xff,0xff, 9,0,0,0,   4,0xff, 5,0, 6,0xff,0xff,0xff, 7,0,0,0])
// //     // test_heap(List(Tuple([U16(),U8()])), [mk_tup(6,7),mk_tup(8,9)], [0,2], [6,0, 7, 0x0ff, 8,0, 9, 0xff])
// //     // test_heap(List(Tuple([Tuple([U16(),U8()]),U8()])), [mk_tup([4,5],6),mk_tup([7,8],9)], [0,2], [4,0, 5,0xff, 6,0xff,  7,0, 8,0xff, 9,0xff])
// // }

// TEST_CASE("Record")
// {
//     auto rt = std::make_shared<record_t>(std::vector<field_ptr>{std::make_shared<field_t>("x", uint8_t()), std::make_shared<field_t>("y", uint16_t()), std::make_shared<field_t>("z", uint32_t())});
//     auto r = std::make_shared<record_t>(std::vector<field_ptr>{std::make_shared<field_t>("x", (uint8_t)1), std::make_shared<field_t>("y", (uint16_t)2), std::make_shared<field_t>("z", (uint32_t)3)});
//     test(rt, {1, 2, 3}, r);
// }

// TEST_CASE("pairs")
// {
//     test_pairs(bool(), {{0, false}, {1, true}, {2, true}, {(int32_t)4294967295, true}});
//     test_pairs(uint8_t(), {{127, (uint8_t)127}, {128, (uint8_t)128}, {255, (uint8_t)255}, {256, (uint8_t)0}, {(int32_t)4294967295, (uint8_t)255}, {(int32_t)4294967168, (uint8_t)128}, {(int32_t)4294967167, (uint8_t)127}});
//     test_pairs(int8_t(), {{127, (int8_t)127}, {128, (int8_t)-128}, {255, (int8_t)-1}, {256, (int8_t)0}, {(int32_t)4294967295, (int8_t)-1}, {(int32_t)4294967168, (int8_t)-128}, {(int32_t)4294967167, (int8_t)127}});
//     test_pairs(uint16_t(), {{32767, (uint16_t)32767}, {32768, (uint16_t)32768}, {65535, (uint16_t)65535}, {65536, (uint16_t)0}, {(1 << 32) - 1, (uint16_t)65535}, {(1 << 32) - 32768, (uint16_t)32768}, {(1 << 32) - 32769, (uint16_t)32767}});
//     test_pairs(int16_t(), {{32767, (int16_t)32767}, {32768, (int16_t)-32768}, {65535, (int16_t)-1}, {65536, (int16_t)0}, {(1 << 32) - 1, (int16_t)-1}, {(1 << 32) - 32768, (int16_t)-32768}, {(1 << 32) - 32769, (int16_t)32767}});
//     test_pairs(uint32_t(), {{(1 << 31) - 1, (uint32_t)((1 << 31) - 1)}, {1 << 31, (uint32_t)(1 << 31)}, {((1 << 32) - 1), (uint32_t)((1 << 32) - 1)}});
//     test_pairs(int32_t(), {{(1 << 31) - 1, (1 << 31) - 1}, {1 << 31, -(1 << 31)}, {(1 << 32) - 1, -1}});
//     // test_pairs(uint64_t(), {{(int64_t)((1ULL << 63) - 1), (uint64_t)((1ULL << 63) - 1)}, {(uint64_t)(1ULL << 63), (uint64_t)(1ULL << 63)}, {(uint64_t)((1ULL << 64) - 1), (uint64_t)((1ULL << 64) - 1)}});
//     // test_pairs(int64_t(), {{(int64_t)((1 << 63) - 1), (int64_t)((1 << 63) - 1)}, {(int64_t)(1 << 63), (int64_t) - (1 << 63)}, {(int64_t)((1 << 64) - 1), (int64_t)-1}});
//     test_pairs(float32_t(), {{(float32_t)3.14, (float32_t)3.14}});
//     test_pairs(float64_t(), {{3.14, 3.14}});
//     test_pairs(wchar_t(), {{0, L'\x00'}, {65, L'A'}, {0xD7FF, L'\uD7FF'}});
//     test_pairs(wchar_t(), {{0xE000, L'\uE000'}, {0x10FFFF, L'\U0010FFFF'}});

//     // test_pairs(Enum([ 'a', 'b' ]), {{0, {'a' : None}}, {1, {'b' : None}}, {2, None}});
// }

// TEST_CASE("Variant")
// {
//     auto t = std::make_shared<variant_t>(std::vector<case_ptr>{std::make_shared<case_t>("x", uint8_t()), std::make_shared<case_t>("y", float64_t()), std::make_shared<case_t>("z", bool())});
//     std::vector<WasmVal> vals_to_lift = {0, 42};
//     auto zero = vals_to_lift[0].index();
//     auto one = vals_to_lift[1].index();
//     test(t, {0, 42}, std::make_shared<variant_t>(std::vector<case_ptr>{std::make_shared<case_t>("x", (uint8_t)42)}));
//     test(t, {0, 256}, std::make_shared<variant_t>(std::vector<case_ptr>{std::make_shared<case_t>("x", (uint8_t)0)}));
//     test(t, {1, (int64_t)0x4048f5c3}, std::make_shared<variant_t>(std::vector<case_ptr>{std::make_shared<case_t>("y", (float64_t)3.140000104904175)}));
//     test(t, {2, false}, std::make_shared<variant_t>(std::vector<case_ptr>{std::make_shared<case_t>("z", false)}));

//     t = std::make_shared<variant_t>(std::vector<case_ptr>{std::make_shared<case_t>("w", uint8_t()), std::make_shared<case_t>("x", uint8_t(), "w"), std::make_shared<case_t>("y", uint8_t()), std::make_shared<case_t>("z", uint8_t(), "x")});
//     test(t, {0, 42}, std::make_shared<variant_t>(std::vector<case_ptr>{std::make_shared<case_t>("w", (uint8_t)42)}));
//     test(t, {1, 42}, std::make_shared<variant_t>(std::vector<case_ptr>{std::make_shared<case_t>("x|w", (uint8_t)42)}));
//     test(t, {2, 42}, std::make_shared<variant_t>(std::vector<case_ptr>{std::make_shared<case_t>("y", (uint8_t)42)}));
//     test(t, {3, 42}, std::make_shared<variant_t>(std::vector<case_ptr>{std::make_shared<case_t>("z|x|w", (uint8_t)42)}));
// }

// // TEST_CASE("Variant2")
// // {
// //     auto t = std::make_shared<variant_t>(std::vector<case_ptr>{std::make_shared<case_t>("w", uint8_t()), std::make_shared<case_t>("x", uint8_t(), "w"), std::make_shared<case_t>("y", uint8_t()), std::make_shared<case_t>("z", uint8_t(), "x")});
// //     auto t2 = std::make_shared<variant_t>(std::vector<case_ptr>{std::make_shared<case_t>("w", uint8_t())});
// //     test(t, {1, 42}, std::make_shared<variant_t>(std::vector<case_ptr>{std::make_shared<case_t>("x|w", (uint8_t)42)}), t2, std::make_shared<variant_t>(std::vector<case_ptr>{std::make_shared<case_t>("w", (uint8_t)42)}));
// //     // test(t, {3, 42}, std::make_shared<variant_t>(std::vector<case_ptr>{std::make_shared<case_t>("z|x|w", (uint8_t)42)}), t2, std::make_shared<variant_t>(std::vector<case_ptr>{std::make_shared<case_t>("w", (uint8_t)42)}));
// //     test(t, {0, 42}, std::make_shared<variant_t>(std::vector<case_ptr>{std::make_shared<case_t>("w", (uint8_t)42)}), t2, std::make_shared<variant_t>(std::vector<case_ptr>{std::make_shared<case_t>("w", (uint8_t)42)}));
// // }
