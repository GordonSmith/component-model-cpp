#include "traits.hpp"
#include "lift.hpp"
#include "lower.hpp"
#include "util.hpp"

using namespace cmcpp;

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <doctest/doctest.h>

#include <iostream>
#include <vector>
#include <utility>
#include <fmt/core.h>

using namespace cmcpp;

class Heap
{
private:
    mutable std::size_t last_alloc;

public:
    std::vector<uint8_t> memory;

    Heap(size_t arg) : memory(arg), last_alloc(0)
    {
        CHECK(true);
    }

    void *realloc(void *original_ptr, size_t original_size, size_t alignment, size_t new_size) const
    {
        if (original_ptr != nullptr && new_size < original_size)
        {
            return align_to(original_ptr, alignment);
        }
        void *ret = align_to(reinterpret_cast<void *>(last_alloc), alignment);
        last_alloc = reinterpret_cast<size_t>(ret) + new_size;
        if (last_alloc > memory.size())
        {
            std::cout << "oom: have " << memory.size() << " need " << last_alloc << std::endl;
            CHECK(false);
            // trap(); // You need to implement this function
        }
        std::copy_n(static_cast<uint8_t *>(original_ptr), original_size, static_cast<uint8_t *>(ret));
        return ret;
    }

private:
    void *align_to(void *ptr, size_t alignment) const
    {
        return reinterpret_cast<void *>((reinterpret_cast<uintptr_t>(ptr) + alignment - 1) & ~(alignment - 1));
    }
};

static std::vector<uint8_t> g_memory;
std::vector<uint8_t> &globalMemory()
{
    g_memory.resize(1024 * 1024);
    return g_memory;
}
CallContextPtr mk_cx(std::vector<uint8_t> &memory = globalMemory(),
                     HostEncoding encoding = HostEncoding::Utf8,
                     const GuestRealloc &realloc = nullptr,
                     const GuestPostReturn &post_return = nullptr)
{
    return createCallContext(memory, realloc, encodeTo, encoding, post_return);
}
void test(const Ref &t, std::vector<WasmVal> vals_to_lift, std::optional<Val> v = std::nullopt, std::optional<Ref> lower_t = std::nullopt, std::optional<Val> lower_v = std::nullopt, const CallContextPtr &cx = mk_cx())
{
    auto vi = CoreValueIter(vals_to_lift);
    if (!v.has_value())
    {
        CHECK_THROWS(lift_flat(*cx, vi, type(t)));
    }

    Val got = lift_flat(*cx, vi, refType(t));
    CHECK(vi.i == vi.values.size());

    const char *got_type = valTypeName(valType(got));
    const char *expected_type = valTypeName(valType(v.value()));
    CHECK(valType(got) == valType(v.value()));

    // fmt.print("got: {},  v: {}", valTypeName(valType(got)), valTypeName(valType(v.value())));
    auto got32 = valType(got) == ValType::U32 ? std::get<uint32_t>(got) : 0;
    auto expected32 = valType(got) == ValType::U32 ? std::get<uint32_t>(v.value()) : 0;
    auto gotChar = valType(got) == ValType::Char ? std::get<wchar_t>(got) : 0;
    auto expectedChar = valType(got) == ValType::Char ? std::get<wchar_t>(v.value()) : 0;
    auto gotString = valType(got) == ValType::String ? std::get<string_ptr>(got)->to_string() : "";
    auto expectedString = valType(got) == ValType::String ? std::get<string_ptr>(v.value())->to_string() : "";

    CHECK(got == v.value());

    if (!lower_t.has_value())
    {
        lower_t = t;
        CHECK(lower_t.value() == t);
    }
    if (!lower_v.has_value())
    {
        lower_v = v;
        CHECK(lower_v.value() == v.value());
    }
    Heap heap(5 * cx->opts->memory.size());
    std::function<int(int, int, int, int)> realloc = [heap](int original_ptr, int original_size, int alignment, int new_size) -> int
    {
        void *retVal = heap.realloc(reinterpret_cast<void *>(original_ptr), original_size, alignment, new_size);
        return reinterpret_cast<std::intptr_t>(retVal);
    };
    CallContextPtr cx2 = createCallContext(heap.memory, realloc, encodeTo, cx->opts->string_encoding);
    auto lowered_vals = lower_flat(*cx2, v.value());
    auto vi2 = CoreValueIter(lowered_vals);
    got = lift_flat(*cx2, vi2, refType(lower_t.value()));
    CHECK(valType(got) == refType(lower_t.value()));
    CHECK(got == lower_v.value());
}

using WasmValValPair = std::pair<WasmVal, std::optional<Val>>;
void test_pairs(const Ref &t, std::vector<WasmValValPair> pairs)
{
    for (auto pair : pairs)
    {
        const char *wasm_type = valTypeName(wasmValType(pair.first));
        const char *expected_type = pair.second.has_value() ? valTypeName(valType(pair.second.value())) : "";
        uint32_t wasm32 = wasmValType(pair.first) == ValType::S32 ? std::get<int32_t>(pair.first) : 0;
        uint32_t val32 = pair.second.has_value() ? valType(pair.second.value()) == ValType::U32 ? std::get<uint32_t>(pair.second.value()) : 0 : 0;
        uint64_t wasm64 = wasmValType(pair.first) == ValType::S64 ? std::get<int64_t>(pair.first) : 0;
        uint64_t val64 = pair.second.has_value() ? valType(pair.second.value()) == ValType::U64 ? std::get<uint64_t>(pair.second.value()) : 0 : 0;
        int64_t valS64 = pair.second.has_value() ? valType(pair.second.value()) == ValType::S64 ? std::get<int64_t>(pair.second.value()) : 0 : 0;
        test(t, {pair.first}, pair.second);
    }
}

template <typename T>
void testValPair(const std::pair<const Val &, T> &pair)
{
    CHECK(std::get<T>(pair.first) == pair.second);
}

template <typename T>
void testValVector(const std::vector<std::pair<const Val &, T>> &pairs)
{
    for (auto pair : pairs)
    {
        testValPair<T>(pair);
    }
}

void testWasmValVal(const std::vector<WasmValValPair> &pairs)
{
    for (const WasmValValPair &pair : pairs)
    {
        const char *wasm_type = valTypeName(wasmValType(pair.first));
        const char *expected_type = valTypeName(valType(pair.second.value()));
        CHECK(true);
    }
}

template <typename T>
void testVal(const Val &v, T expected)
{
    CHECK(std::get<T>(v) == expected);
    testValPair<T>({v, expected});
    testValVector<T>({{v, expected}});
    testWasmValVal({{expected, v}});
}

TEST_CASE("pairs")
{
    test_pairs(Bool(), {{0, false}, {1, true}, {2, true}, {(int32_t)4294967295, true}});
    test_pairs(U8(), {{127, (uint8_t)127}, {128, (uint8_t)128}, {255, (uint8_t)255}, {256, (uint8_t)0}, {(int32_t)4294967295, (uint8_t)255}, {(int32_t)4294967168, (uint8_t)128}, {(int32_t)4294967167, (uint8_t)127}});
    test_pairs(S8(), {{127, (int8_t)127}, {128, (int8_t)-128}, {255, (int8_t)-1}, {256, (int8_t)0}, {(int32_t)4294967295, (int8_t)-1}, {(int32_t)4294967168, (int8_t)-128}, {(int32_t)4294967167, (int8_t)127}});
    test_pairs(U16(), {{32767, (uint16_t)32767}, {32768, (uint16_t)32768}, {65535, (uint16_t)65535}, {65536, (uint16_t)0}, {(1 << 32) - 1, (uint16_t)65535}, {(1 << 32) - 32768, (uint16_t)32768}, {(1 << 32) - 32769, (uint16_t)32767}});
    test_pairs(S16(), {{32767, (int16_t)32767}, {32768, (int16_t)-32768}, {65535, (int16_t)-1}, {65536, (int16_t)0}, {(1 << 32) - 1, (int16_t)-1}, {(1 << 32) - 32768, (int16_t)-32768}, {(1 << 32) - 32769, (int16_t)32767}});
    test_pairs(U32(), {{(1 << 31) - 1, (uint32_t)((1 << 31) - 1)}, {1 << 31, (uint32_t)(1 << 31)}, {((1 << 32) - 1), (uint32_t)((1 << 32) - 1)}});
    test_pairs(S32(), {{(1 << 31) - 1, (1 << 31) - 1}, {1 << 31, -(1 << 31)}, {(1 << 32) - 1, -1}});
    test_pairs(U64(), {{(int64_t)((1ULL << 63) - 1), (uint64_t)((1ULL << 63) - 1)}, {(int64_t)(1ULL << 63), (uint64_t)(1ULL << 63)}, {(int64_t)((1ULL << 64) - 1), (uint64_t)((1ULL << 64) - 1)}});
    test_pairs(S64(), {{(int64_t)((1 << 63) - 1), (int64_t)((1 << 63) - 1)}, {(int64_t)(1 << 63), (int64_t) - (1 << 63)}, {(int64_t)((1 << 64) - 1), (int64_t)-1}});
    test_pairs(F32(), {{(float32_t)3.14, (float32_t)3.14}});
    test_pairs(F64(), {{3.14, 3.14}});
    test_pairs(Char(), {{0, L'\x00'}, {65, L'A'}, {0xD7FF, L'\uD7FF'}});
    test_pairs(Char(), {{0xE000, L'\uE000'}, {0x10FFFF, L'\U0010FFFF'}});

    // test_pairs(Enum([ 'a', 'b' ]), {{0, {'a' : None}}, {1, {'b' : None}}, {2, None}});
}

void test_string_internal(HostEncoding host_encoding, GuestEncoding guest_encoding, const std::string &guestS, const std::string &expected)
{
    Heap heap(guestS.length() * 2);
    memcpy(heap.memory.data(), guestS.data(), guestS.length());

    auto cx = mk_cx(heap.memory, host_encoding);

    test(String(), {0, (int32_t)guestS.length()}, std::make_shared<string_t>(expected), std::nullopt, std::nullopt, cx);
}

TEST_CASE("Strings")
{
    std::vector<HostEncoding> host_encodings = {HostEncoding::Utf8}; //, HostEncoding::Utf16, HostEncoding::Latin1_Utf16};
    std::vector<std::string> fun_strings = {"", "a", "hi", "\x00", "a\x00b", "\x80", "\x80b", "ab\xefc", "\u01ffy", "xy\u01ff", "a\ud7ffb", "a\u02ff\u03ff\u04ffbc", "\uf123", "\uf123\uf123abc", "abcdef\uf123"};
    for (auto s : fun_strings)
    {
        for (auto h_enc : host_encodings)
        {
            test_string_internal(h_enc, GuestEncoding::Utf8, s, s);
        }
    }
}
