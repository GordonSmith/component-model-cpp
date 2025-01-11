#ifndef CMCPP_UTIL_HPP
#define CMCPP_UTIL_HPP

#include "context.hpp"

namespace cmcpp
{
    const bool DETERMINISTIC_PROFILE = false;

    void trap_if(const CallContext &cx, bool condition, const char *message = nullptr) noexcept(false);

    uint32_t align_to(uint32_t ptr, uint8_t alignment);

    bool_t convert_int_to_bool(uint8_t i);

    char_t convert_i32_to_char(const CallContext &cx, int32_t i);
    int32_t char_to_i32(const CallContext &cx, const char_t &v);
}

#endif
