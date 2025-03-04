#ifndef CMCPP_TUPLE_HPP
#define CMCPP_TUPLE_HPP

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
    namespace tuple
    {

        template <Tuple T>
        void store(const CallContext &cx, const T &v, uint32_t ptr)
        {
            auto process_field = [&](auto &&field)
            {
                ptr = align_to(ptr, ValTrait<std::remove_reference_t<decltype(field)>>::alignment);
                store(cx, field, ptr);
                ptr += ValTrait<std::remove_reference_t<decltype(field)>>::size;
            };

            std::apply([&](auto &&...fields)
                       { (process_field(fields), ...); }, v);
        }

        template <Tuple T>
        WasmValVector lower_flat_tuple(CallContext &cx, const T &v)
        {
            WasmValVector retVal = {};
            auto process_field = [&](auto &&field)
            {
                auto flat = lower_flat(cx, field);
                retVal.insert(retVal.end(), flat.begin(), flat.end());
            };

            std::apply([&](auto &&...fields)
                       { (process_field(fields), ...); }, v);
            return retVal;
        }

        template <Tuple T>
        T load(const CallContext &cx, uint32_t ptr)
        {
            T result;
            auto process_field = [&](auto &&field)
            {
                ptr = align_to(ptr, ValTrait<std::remove_reference_t<decltype(field)>>::alignment);
                field = load<std::remove_reference_t<decltype(field)>>(cx, ptr);
                ptr += ValTrait<std::remove_reference_t<decltype(field)>>::size;
            };

            std::apply([&](auto &&...fields)
                       { (process_field(fields), ...); }, result);
            return result;
        }

        template <Tuple T>
        inline T lift_flat_tuple(const CallContext &cx, const CoreValueIter &vi)
        {
            T result;
            auto process_field = [&](auto &&field)
            {
                field = lift_flat<std::remove_reference_t<decltype(field)>>(cx, vi);
            };

            std::apply([&](auto &&...fields)
                       { (process_field(fields), ...); }, result);
            return result;
        }
    }

    template <Tuple T>
    inline void store(CallContext &cx, const T &v, uint32_t ptr)
    {
        tuple::store(cx, v, ptr);
    }

    template <Tuple T>
    inline WasmValVector lower_flat(CallContext &cx, const T &v)
    {
        return tuple::lower_flat_tuple(cx, v);
    }

    template <Record T>
    inline WasmValVector lower_flat(CallContext &cx, const T &v)
    {
        return tuple::lower_flat_tuple(cx, boost::pfr::structure_to_tuple((typename ValTrait<T>::inner_type)v));
    }

    template <Tuple T>
    inline T load(const CallContext &cx, uint32_t ptr)
    {
        return tuple::load<T>(cx, ptr);
    }

    template <Tuple T>
    inline T lift_flat(const CallContext &cx, const CoreValueIter &vi)
    {
        auto x = tuple::lift_flat_tuple<T>(cx, vi);
        return x;
    }

    template <Record T>
    inline T lift_flat(const CallContext &cx, const CoreValueIter &vi)
    {
        using TupleType = decltype(boost::pfr::structure_to_tuple(std::declval<typename ValTrait<T>::inner_type>()));
        TupleType x = tuple::lift_flat_tuple<TupleType>(cx, vi);
        return to_struct<T>(x);
    }

}

#endif
