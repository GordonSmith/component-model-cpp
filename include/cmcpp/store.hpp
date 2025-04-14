#ifndef CMCPP_STORE_HPP
#define CMCPP_STORE_HPP

#include "context.hpp"

namespace cmcpp
{
    template <Boolean T>
    inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr);

    template <Char T>
    inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr);

    template <Integer T>
    inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr);

    template <Float T>
    inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr);

    template <String T>
    inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr);

    template <Flags T>
    inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr);

    template <List T>
    inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr);

    template <Tuple T>
    inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr);

    template <Record T>
    inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr);

    template <Variant T>
    inline void store(LiftLowerContext &cx, const T &v, uint32_t ptr);
}

#endif
