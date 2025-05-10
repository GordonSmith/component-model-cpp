#ifndef CMCPP_TUPLE_HPP
#define CMCPP_TUPLE_HPP

#include "context.hpp"
#include "integer.hpp"
#include "store.hpp"
#include "lower.hpp"
#include "load.hpp"
#include "lift.hpp"
#include "util.hpp"

namespace cmcpp
{
    namespace tuple
    {

        template <Tuple T>
        void store(LiftLowerContext &cx, const T &v, uint32_t ptr)
        {
            auto process_field = [&](auto &&field)
            {
                using field_type = std::remove_const_t<std::remove_reference_t<decltype(field)>>;
                ptr = align_to(ptr, ValTrait<field_type>::alignment);
                cmcpp::store(cx, field, ptr);
                ptr += ValTrait<field_type>::size;
            };

            std::apply([&](auto &&...fields)
                       { (process_field(fields), ...); }, v);
        }

        template <Tuple T>
        WasmValVector lower_flat(LiftLowerContext &cx, const T &v)
        {
            WasmValVector retVal = {};
            auto process_field = [&](auto &&field)
            {
                auto flat = cmcpp::lower_flat(cx, field);
                retVal.insert(retVal.end(), flat.begin(), flat.end());
            };

            std::apply([&](auto &&...fields)
                       { (process_field(fields), ...); }, v);
            return retVal;
        }

        template <Tuple T>
        T load(const LiftLowerContext &cx, uint32_t ptr)
        {
            T result;
            auto process_field = [&](auto &&field)
            {
                ptr = align_to(ptr, ValTrait<std::remove_reference_t<decltype(field)>>::alignment);
                field = cmcpp::load<std::remove_reference_t<decltype(field)>>(cx, ptr);
                ptr += ValTrait<std::remove_reference_t<decltype(field)>>::size;
            };

            std::apply([&](auto &&...fields)
                       { (process_field(fields), ...); }, result);
            return result;
        }

        template <Tuple T>
        inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
        {
            T result;
            auto process_field = [&](auto &&field)
            {
                field = cmcpp::lift_flat<std::remove_reference_t<decltype(field)>>(cx, vi);
            };

            std::apply([&](auto &&...fields)
                       { (process_field(fields), ...); }, result);
            return result;
        }
    }

    template <Tuple T>
    inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr)
    {
        tuple::store(cx, v, ptr);
    }

    template <Tuple T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v)
    {
        return tuple::lower_flat(cx, v);
    }

    template <Record T>
    inline WasmValVector lower_flat(LiftLowerContext &cx, const T &v)
    {
        return tuple::lower_flat(cx, boost::pfr::structure_to_tuple((typename ValTrait<T>::inner_type)v));
    }

    template <Tuple T>
    inline T load(const LiftLowerContext &cx, uint32_t ptr)
    {
        return tuple::load<T>(cx, ptr);
    }

    template <Tuple T>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
    {
        return tuple::lift_flat<T>(cx, vi);
    }

    template <Record T>
    inline T lift_flat(const LiftLowerContext &cx, const CoreValueIter &vi)
    {
        using TupleType = decltype(boost::pfr::structure_to_tuple(std::declval<typename ValTrait<T>::inner_type>()));
        TupleType x = tuple::lift_flat<TupleType>(cx, vi);
        return to_struct<T>(x);
    }

}

#endif
