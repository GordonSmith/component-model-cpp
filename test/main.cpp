#include "traits.hpp"
#include "lift.hpp"

using namespace cmcpp;

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <doctest/doctest.h>

#include <iostream>
#include <vector>
#include <utility>

using namespace cmcpp;

cmcpp::CallContextPtr mk_cx(std::vector<uint8_t> memory = std::vector<uint8_t>(),
                            cmcpp::HostEncoding encoding = cmcpp::HostEncoding::Utf8,
                            const cmcpp::GuestRealloc &realloc = nullptr,
                            const cmcpp::GuestPostReturn &post_return = nullptr)
{
    return createCallContext(memory, realloc, encodeTo, encoding, post_return);
}

void test(const Ref &t, const std::vector<WasmVal> &vals_to_lift, std::optional<Val> v, const CallContextPtr &cx = mk_cx())
{
    auto vi = CoreValueIter(vals_to_lift);
    if (!v.has_value())
    {
        CHECK_THROWS(lift_flat(*cx, vi, type(t)));
    }
}

void test_pairs(const Ref &t, std::vector<std::pair<const WasmVal &, const Val &>> pairs)
{
    for (auto pair : pairs)
    {
        test(t, {pair.first}, pair.second);
    }
}

TEST_CASE("pairs")
{
    test_pairs(Bool(), {{0, false}, {1, true}, {2, true}, {4294967295, true}});
}