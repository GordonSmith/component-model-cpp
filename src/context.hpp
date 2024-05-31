#ifndef CMCPP_HPP
#define CMCPP_HPP

#if __has_include(<span>)
#include <span>
#else
#include <string>
#include <sstream>
#endif

#include <string>
#include <functional>
#include <memory>

namespace cmcpp
{

    enum class HostEncoding
    {
        Utf8,
        Utf16,
        Latin1,
        Latin1_Utf16
    };

    enum class GuestEncoding
    {
        Utf8,
        Utf16le,
        Latin1
    };

    using GuestMemory = std::span<uint8_t, std::dynamic_extent>;
    using GuestRealloc = std::function<int(int ptr, int old_size, int align, int new_size)>;
    using GuestPostReturn = std::function<void()>;
    using HostEncodeTo = std::function<size_t(void *dest, const char8_t *src, uint32_t byte_len, GuestEncoding encoding)>;

    class CanonicalOptions
    {
    public:
        virtual ~CanonicalOptions() = default;

        GuestMemory memory;
        HostEncoding string_encoding;

        virtual int realloc(int ptr, int old_size, int align, int new_size) = 0;
        virtual size_t encodeTo(void *dest, const char8_t *src, uint32_t byte_len, GuestEncoding encoding) = 0;
        virtual void post_return() = 0;
    };
    using CanonicalOptionsPtr = std::shared_ptr<CanonicalOptions>;
    CanonicalOptionsPtr createCanonicalOptions(const GuestMemory &memory, const GuestRealloc &realloc, const HostEncodeTo &encodeTo, HostEncoding encoding, const GuestPostReturn &post_return);

    class CallContext
    {
    public:
        virtual ~CallContext() = default;

        CanonicalOptionsPtr opts;
    };
    using CallContextPtr = std::shared_ptr<CallContext>;
    CallContextPtr createCallContext(const GuestMemory &memory, const GuestRealloc &realloc, const HostEncodeTo &encodeTo, HostEncoding encoding = HostEncoding::Utf8, const GuestPostReturn &post_return = nullptr);
}

#endif