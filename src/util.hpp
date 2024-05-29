#ifndef UTIL_HPP
#define UTIL_HPP

#include "context.hpp"
#include "traits.hpp"

#include <map>

namespace cmcpp
{
    size_t encodeTo(void *dest, const char *src, uint32_t byte_len, GuestEncoding encoding);

    class CoreValueIter
    {

    public:
        std::vector<WasmVal> values;
        size_t i = 0;

        CoreValueIter(const std::vector<WasmVal> &values);
        template <typename T>
        T next()
        {
            return std::get<T>(values[i++]);
        }
        void skip();
    };

}
#endif
