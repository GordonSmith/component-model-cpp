#include "load.hpp"
#include "integer.hpp"
#include "float.hpp"
#include "string.hpp"
#include "flags.hpp"

namespace cmcpp
{
    namespace cmcpp
    {
        template <Boolean T>
        inline T load(const CallContext &cx, uint32_t ptr)
        {
            return convert_int_to_bool(integer::load<uint8_t>(cx, ptr));
        }

        template <Char T>
        inline T load(const CallContext &cx, uint32_t ptr)
        {
            return convert_i32_to_char(cx, integer::load<uint32_t>(cx, ptr));
        }

        template <Integer T>
        inline T load(const CallContext &cx, uint32_t ptr)
        {
            return integer::load<T>(cx, ptr);
        }

        template <Float T>
        inline T load(const CallContext &cx, uint32_t ptr)
        {
            return float_::load<T>(cx, ptr);
        }

        template <String T>
        inline T load(const CallContext &cx, uint32_t ptr)
        {
            return string::load<T>(cx, ptr);
        }

        template <Flags T>
        inline T load(const CallContext &cx, uint32_t ptr)
        {
            return flags::load<T>(cx, ptr);
        }
    }

}
