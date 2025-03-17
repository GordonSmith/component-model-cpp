#ifndef CMCPP_CONTEXT_HPP
#define CMCPP_CONTEXT_HPP

#include "traits.hpp"

#include <functional>
#include <memory>
#include <optional>
#if __has_include(<span>)
#include <span>
#else
#include <string>
#include <sstream>
#endif

namespace cmcpp
{
    using HostTrap = std::function<void(const char *msg) noexcept(false)>;
    using GuestRealloc = std::function<int(int ptr, int old_size, int align, int new_size)>;
    using GuestMemory = std::span<uint8_t>;
    using GuestPostReturn = std::function<void()>;
    using HostUnicodeConversion = std::function<std::pair<void *, size_t>(void *dest, uint32_t dest_byte_len, const void *src, uint32_t src_byte_len, Encoding from_encoding, Encoding to_encoding)>;

    struct CallContext
    {
        HostTrap trap;
        GuestRealloc realloc;
        GuestMemory memory;
        Encoding guest_encoding;
        HostUnicodeConversion convert;
        std::optional<GuestPostReturn> post_return;
        bool sync = true;
        bool always_task_return = false;
    };
    ;

    struct InstanceContext
    {
        HostTrap trap;
        HostUnicodeConversion convert;
        GuestRealloc realloc;
        std::unique_ptr<CallContext> createCallContext(const GuestMemory &memory, const Encoding &guest_encoding = Encoding::Utf8, const GuestPostReturn &post_return = nullptr)
        {
            auto retVal = std::make_unique<CallContext>();
            retVal->trap = trap;
            retVal->convert = convert;
            retVal->realloc = realloc;
            retVal->memory = memory;
            retVal->guest_encoding = guest_encoding;
            retVal->post_return = post_return;
            return retVal;
        }
    };

    inline std::unique_ptr<InstanceContext> createInstanceContext(const HostTrap &trap, HostUnicodeConversion convert, const GuestRealloc &realloc)
    {
        auto retVal = std::make_unique<InstanceContext>();
        retVal->trap = trap;
        retVal->convert = convert;
        retVal->realloc = realloc;
        return retVal;
    }
}

#endif
