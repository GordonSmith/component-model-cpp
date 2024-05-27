#include "context.hpp"
#include "lower.hpp"
#include "lift.hpp"
#include "val.hpp"
#include "util.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <cassert>
#include <algorithm>
#include <stdexcept>

#include <doctest/doctest.h>

bool equal_modulo_string_encoding(const auto &s, const auto &t)
{
    if (s == nullptr && t == nullptr)
    {
        return true;
    }
    if (std::is_same_v<decltype(s), bool> && std::is_same_v<decltype(t), bool> ||
        std::is_same_v<decltype(s), int> && std::is_same_v<decltype(t), int> ||
        std::is_same_v<decltype(s), float> && std::is_same_v<decltype(t), float> ||
        std::is_same_v<decltype(s), std::string> && std::is_same_v<decltype(t), std::string>)
    {
        return s == t;
    }
    if (std::is_same_v<decltype(s), std::tuple<std::string>> && std::is_same_v<decltype(t), std::tuple<std::string>>)
    {
        assert(std::get<0>(s) == std::get<0>(t));
        return std::get<0>(s) == std::get<0>(t);
    }
    // if (std::is_same_v<decltype(s), std::unordered_map<std::string, auto>> && std::is_same_v<decltype(t), std::unordered_map<std::string, auto>>)
    // {
    //     for (const auto &[key, value] : s)
    //     {
    //         if (!equal_modulo_string_encoding(value, t.at(key)))
    //         {
    //             return false;
    //         }
    //     }
    //     return true;
    // }
    // if (std::is_same_v<decltype(s), std::vector<auto>> && std::is_same_v<decltype(t), std::vector<auto>>)
    // {
    //     for (size_t i = 0; i < s.size(); i++)
    //     {
    //         if (!equal_modulo_string_encoding(s[i], t[i]))
    //         {
    //             return false;
    //         }
    //     }
    //     return true;
    // }

    assert(false);
    return false;
}

class Heap
{
private:
    std::vector<uint8_t> memory;
    std::size_t last_alloc;

public:
    Heap(size_t arg) : memory(arg), last_alloc(0) {}

    void *realloc(void *original_ptr, size_t original_size, size_t alignment, size_t new_size)
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
            assert(false);
            // trap(); // You need to implement this function
        }
        std::copy_n(static_cast<uint8_t *>(original_ptr), original_size, static_cast<uint8_t *>(ret));
        return ret;
    }

private:
    void *align_to(void *ptr, size_t alignment)
    {
        return reinterpret_cast<void *>((reinterpret_cast<uintptr_t>(ptr) + alignment - 1) & ~(alignment - 1));
    }
};

cmcpp::CanonicalOptionsPtr mk_opts(std::vector<uint8_t> memory = std::vector<uint8_t>(),
                                   cmcpp::HostEncoding encoding = cmcpp::HostEncoding::Utf8,
                                   const cmcpp::GuestRealloc &realloc = nullptr,
                                   const cmcpp::GuestPostReturn &post_return = nullptr)
{
    return cmcpp::createCanonicalOptions(memory, realloc, cmcpp::encodeTo, encoding, post_return);
}

struct ComponentInstance
{
};

cmcpp::CallContextPtr mk_cx(std::vector<uint8_t> memory = std::vector<uint8_t>(),
                            cmcpp::HostEncoding encoding = cmcpp::HostEncoding::Utf8,
                            const cmcpp::GuestRealloc &realloc = nullptr,
                            const cmcpp::GuestPostReturn &post_return = nullptr)
{
    return cmcpp::createCallContext(memory, realloc, cmcpp::encodeTo, encoding, post_return);
}

std::tuple<std::string, std::string, size_t> mk_str(const std::string &s)
{
    return std::make_tuple(s, "utf8", s.size());
}

void fail(const std::string &msg)
{
    throw std::runtime_error(msg);
}

void test(const cmcpp::ValBase &t, const std::vector<std::variant<int, float>> &vals_to_lift, const std::string &v,
          const cmcpp::CallContextPtr &cx = mk_cx()
          //   ,const auto &dst_encoding = std::string(),
          //   const auto &lower_t = std::optional<decltype(t)>(),
          //   const auto &lower_v = std::optional<decltype(v)>()
)
{
    auto test_name = [&]()
    {
        return "test():";
    };

    cmcpp::CoreValueIter vi(vals_to_lift);

    // if (v == nullptr)
    // {
    //     try
    //     {
    //         auto got = lift_flat(cx, vi, t);
    //         fail(test_name() + " expected trap, but got " + got);
    //     }
    //     catch (const Trap &)
    //     {
    //         return;
    //     }
    // }

    // auto got = cmcpp::lift_flat(*cx, vi, t.t);
    // assert(vi.i == vi.values.size());
    // if (got != v)
    // {
    //     fail(test_name() + " initial lift_flat() expected " + v + " but got " + got);
    // }

    // if (!lower_t.has_value())
    // {
    //     lower_t = t;
    // }
    // if (!lower_v.has_value())
    // {
    //     lower_v = v;
    // }

    // Heap heap(5 * cx.opts.memory.size());
    // if (dst_encoding.empty())
    // {
    //     dst_encoding = cx.opts.string_encoding;
    // }
    // cx = mk_cx(heap.memory, dst_encoding, heap.realloc);
    // auto lowered_vals = lower_flat(cx, v, lower_t);

    // vi = CoreValueIter(lowered_vals);
    // got = lift_flat(cx, vi, lower_t);
    // if (!equal_modulo_string_encoding(got, lower_v))
    // {
    //     fail(test_name() + " re-lift expected " + lower_v + " but got " + got);
    // }
}

TEST_CASE("run_tests.py")
{
    std::cout << "run_tests.py" << std::endl;
    // cmcpp::Field('x', U8()), cmcpp::Field('y', U16()), cmcpp::Field('z', U32())}), cmcpp::List({1, 2, 3}), {'x' : 1, 'y' : 2, 'z' : 3}
    cmcpp::Record r({cmcpp::Field("x", cmcpp::ValType::U8), cmcpp::Field("y", cmcpp::ValType::U16), cmcpp::Field("z", cmcpp::ValType::U32)});
    std::vector<std::variant<int, float>> vals_to_lift({1, 2, 3});
    test(r, {1, 2, 3}, "{'x': 1, 'y': 2, 'z': 3}");
}
