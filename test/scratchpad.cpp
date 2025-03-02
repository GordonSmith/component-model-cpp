#include "traits.hpp"
#include "lift.hpp"
#include "lower.hpp"
#include "util.hpp"
#include "host-util.hpp"

using namespace cmcpp;

#include <doctest/doctest.h>

#include <iostream>
#include <vector>
#include <utility>
#include <cassert>

//  Traits  ---
#include "boost/pfr.hpp"

// Helper function to create a struct from a tuple
template <typename T, typename Tuple, std::size_t... I>
T construct_from_tuple_impl(const Tuple &t, std::index_sequence<I...>)
{
    return T{std::get<I>(t)...};
}

template <typename T, typename Tuple>
T construct_from_tuple(const Tuple &t)
{
    constexpr auto size = std::tuple_size<Tuple>::value;
    return construct_from_tuple_impl<T>(t, std::make_index_sequence<size>{});
}

struct Person
{
    string_t name;
    uint16_t age;
    uint32_t weight;
};

struct PersonEx : Person
{
    string_t address;
};

TEST_CASE("Records")
{
    Heap heap(1024 * 1024);
    auto cx = createCallContext(&heap, Encoding::Utf8);

    PersonEx p = {"John", 42, 200, "123 Main St."};
    auto x = boost::pfr::structure_to_tuple<Person>(p);
    Person y = construct_from_tuple<Person>(x);

    // auto t = struct_to_tuple(p);
    // auto xxx = ValTrait<Person>::get<1>(p);
    // CHECK(ValTrait<Person>::type == ValType::Record);
    // auto v = lower_flat_test(*cx, p);
    // auto rr = lift_flat<Person>(*cx, v);
    // CHECK(r0 == rr);

    //     MyRecord0 r0 = {42, 43};
    //     // auto str0 = to_struct<MyRecord0>(rr);

    //     MyRecord1 r1 = {142, 143, "Hello"};
    //     auto vv = lower_flat(*cx, r1);
    //     auto rr1 = lift_flat<MyRecord1>(*cx, vv);
    //     CHECK(r1 == rr1);
    //     // auto str1 = to_struct<MyRecord1>(rr1);

    //     MyRecord2 r2 = {242, 243, "2Hello", {"2World", "!"}};
    //     auto vvv = lower_flat(*cx, r2);
    //     auto rr2 = lift_flat<MyRecord2>(*cx, vvv);
    //     CHECK(r2 == rr2);
    //     // auto str2 = to_struct<MyRecord2>(rr2);
}
