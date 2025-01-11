#include "util.hpp"

namespace cmcpp
{

    void trap_if(const CallContext &cx, bool condition, const char *message)
    {
        if (condition)
        {
            cx.trap(message);
        }
    }

    uint32_t align_to(uint32_t ptr, uint8_t alignment)
    {
        return (ptr + static_cast<int32_t>(alignment) - 1) & ~(static_cast<int32_t>(alignment) - 1);
    }

    ValType despecialize(const ValType t)
    {
        switch (t)
        {
        case ValType::Tuple:
            return ValType::Record;
        case ValType::Enum:
            return ValType::Variant;
        case ValType::Option:
            return ValType::Variant;
        case ValType::Result:
            return ValType::Variant;
        }
        return t;
    }

    bool convert_int_to_bool(uint8_t i)
    {
        return i > 0;
    }

    char_t convert_i32_to_char(const CallContext &cx, int32_t i)
    {
        trap_if(cx, i >= 0x110000);
        trap_if(cx, 0xD800 <= i && i <= 0xDFFF);
        return i;
    }

    int32_t char_to_i32(const CallContext &cx, const char_t &v)
    {
        uint32_t retVal = v;
        trap_if(cx, retVal >= 0x110000);
        trap_if(cx, 0xD800 <= retVal && retVal <= 0xDFFF);
        return retVal;
    }

}