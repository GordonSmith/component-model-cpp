#ifndef CMCPP_VARIANT_HPP
#define CMCPP_VARIANT_HPP

#include "context.hpp"
#include "integer.hpp"
#include "store.hpp"
#include "load.hpp"
#include "util.hpp"

#include <tuple>
#include <limits>
#include <cassert>

namespace cmcpp
{
    namespace variant
    {
        // template <Variant T>
        // std::pair<int, std::optional<typename T::value_type>> match_case(const T &v)
        // {
        //     auto index = v.index();
        //     auto value = std::get<index>(v);
        //     return {index, value};
        // }

        // template <Variant T>
        // ValType discriminant_type(const T &v)
        // {
        //     auto n = std::variant_size_v<decltype(v)>;
        //     assert(n > 0 && n < std::numeric_limits<unsigned int>::max());
        //     int match = std::ceil<int>(std::log2(n) / 8);
        //     switch (match)
        //     {
        //     case 0:
        //         return ValType::U8;
        //     case 1:
        //         return ValType::U8;
        //     case 2:
        //         return ValType::U16;
        //     case 3:
        //         return ValType::U32;
        //     }
        //     return ValType::UNKNOWN;
        // }

        // template <Variant T>
        // std::size_t max_case_alignment(const T &v)
        // {
        //     std::size_t a = 1;
        //     std::visit([&](auto &&arg)
        //                {
        //         using V = std::decay_t<decltype(arg)>;
        //         a = std::max(a, alignment(arg)); }, v);
        //     return a;
        // }

        // template <Variant T>
        // std::size_t alignment_variant(const T &v)
        // {
        //     return std::max(alignment(discriminant_type(v)), max_case_alignment(v));
        // }

        // template <Variant T>
        // WasmValVector flatten_variant(const T &v)
        // {
        //     WasmValVector flat;
        //     std::visit([&](auto &&arg) {
        //         using V = std::decay_t<decltype(arg)>;
        //         auto f = lower_flat(arg);
        //         flat.insert(flat.end(), f.begin(), f.end()); 
        //     }, v);
        // }


        // template <Variant T>
        // void lower_flat(CallContext &cx, const T &v, uint32_t ptr)
        // {
        //     auto [case_index, case_value] = match_case(v);
        //     std::vector<std::string> flat_types = flatten_variant(v);
        //     assert(flat_types[0] == "i32");
        //     flat_types.erase(flat_types.begin());
        //     auto c = cases[case_index];
        //     std::vector<WasmVal> payload;
        //     if (c->v.has_value())
        //     {
        //         payload = lower_flat(cx, case_value.value(), c->v.value());
        //         auto vTypes = flatten_type(c->v.value());
        //         for (size_t i = 0; i < payload.size(); ++i)
        //         {
        //             auto fv = payload[i];
        //             auto have = vTypes[i];
        //             auto want = flat_types[0];
        //             flat_types.erase(flat_types.begin());
        //             if (have == "f32" && want == "i32")
        //             {
        //                 payload[i] = encode_float_as_i32(std::get<float32_t>(fv));
        //             }
        //             else if (have == "i32" && want == "i64")
        //             {
        //                 payload[i] = fv;
        //             }
        //             else if (have == "f32" && want == "i64")
        //             {
        //                 payload[i] = encode_float_as_i32(std::get<float32_t>(fv));
        //             }
        //             else if (have == "f64" && want == "i64")
        //             {
        //                 payload[i] = static_cast<int64_t>(encode_float_as_i64(std::get<float64_t>(fv)));
        //             }
        //             else
        //             {
        //                 assert(have == want);
        //             }
        //         }
        //     }
        //     for (auto _ : flat_types)
        //     {
        //         payload.push_back(0);
        //     }
        //     payload.insert(payload.begin(), case_index);
        //     return payload;
        // }
    }
}

#endif
