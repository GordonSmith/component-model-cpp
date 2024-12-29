#ifndef CMCPP_BOOLEAN_HPP
#define CMCPP_BOOLEAN_HPP

#include "context.hpp"
#include "integer.hpp"
#include "util.hpp"

namespace cmcpp
{

    namespace boolean
    {
        void store(CallContext &cx, const bool_t &v, offset ptr)
        {
            integer::store<uint8_t>(cx, v, ptr);
        }

        bool_t load(const CallContext &cx, uint32_t ptr)
        {
            convert_int_to_bool(integer::load<uint8_t>(cx, ptr));
        }
    }
}

#endif
