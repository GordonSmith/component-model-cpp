#ifndef CMCPP_LOAD_HPP
#define CMCPP_LOAD_HPP

#include "context.hpp"

namespace cmcpp
{
    template <Boolean T>
    inline T load(const LiftLowerContext &cx, uint32_t ptr);

    template <Char T>
    inline T load(const LiftLowerContext &cx, uint32_t ptr);

    template <Integer T>
    inline T load(const LiftLowerContext &cx, uint32_t ptr);

    template <Float T>
    inline T load(const LiftLowerContext &cx, uint32_t ptr);

    template <String T>
    inline T load(const LiftLowerContext &cx, uint32_t ptr);

    template <Flags T>
    inline T load(const LiftLowerContext &cx, uint32_t ptr);

    template <List T>
    inline T load(const LiftLowerContext &cx, uint32_t ptr);

    template <Tuple T>
    inline T load(const LiftLowerContext &cx, uint32_t ptr);

    template <Record T>
    inline T load(const LiftLowerContext &cx, uint32_t ptr);

    template <Variant T>
    inline T load(const LiftLowerContext &cx, uint32_t ptr);
}

#include "string.hpp"

#endif
