#ifndef CMCPP_RECORD_HPP
#define CMCPP_RECORD_HPP

#include "context.hpp"
#include "store.hpp"
#include "load.hpp"
#include "tuple.hpp"
#include "util.hpp"
#include <boost/pfr.hpp>

namespace cmcpp
{
    // Helper to convert tuple to struct (works with plain structs, not just record_t)
    template <Struct S, Tuple T, std::size_t... I>
    S tuple_to_struct_impl(const T &t, std::index_sequence<I...>)
    {
        return S{std::get<I>(t)...};
    }

    template <Struct S, Tuple T>
    S tuple_to_struct(const T &t)
    {
        return tuple_to_struct_impl<S>(t, std::make_index_sequence<std::tuple_size_v<T>>{});
    }

    // Store a record to WebAssembly linear memory
    // Records are stored as tuples of their fields using Boost PFR for reflection
    template <Record T>
    inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr)
    {
        // record_t<R> inherits from R, so we need to work with the base struct
        using base_type = typename ValTrait<T>::inner_type;
        const base_type &base = static_cast<const base_type &>(v);

        // Convert the base struct to a tuple using Boost PFR reflection
        auto tuple_value = boost::pfr::structure_to_tuple(base);

        // Store the tuple (which handles all fields sequentially)
        tuple::store(cx, tuple_value, ptr);
    }

    // Load a record from WebAssembly linear memory
    // Records are loaded as tuples of their fields using Boost PFR for reflection
    template <Record T>
    inline T load(const LiftLowerContext &cx, uint32_t ptr)
    {
        // Get the tuple type for this record
        using tuple_type = typename ValTrait<T>::tuple_type;

        // Load as a tuple
        auto tuple_value = tuple::load<tuple_type>(cx, ptr);

        // Convert tuple back to the base struct
        using base_type = typename ValTrait<T>::inner_type;
        base_type base = tuple_to_struct<base_type>(tuple_value);

        // Construct the record_t wrapper from the base
        T result;
        static_cast<base_type &>(result) = base;
        return result;
    }

} // namespace cmcpp

#endif // CMCPP_RECORD_HPP
