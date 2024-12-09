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
    enum class PythonEncoding
    {
        latin_1,
        utf_8,
        utf_16_le,
        Latin1_Utf16
    };

    enum class Encoding
    {
        Latin1,
        Utf8,
        Utf16,
        Latin1_Utf16
    };

    using GuestMemory = std::span<uint8_t, std::dynamic_extent>;
    using GuestRealloc = std::function<int(int ptr, int old_size, int align, int new_size)>;
    using GuestPostReturn = std::function<void()>;
    using HostTrap = std::function<void(const char *msg)>;
    using HostUnicodeConversion = std::function<std::pair<char8_t *, size_t>(char8_t *dest, const char8_t *src, uint32_t byte_len, Encoding from_encoding, Encoding to_encoding)>;

    class CanonicalOptions
    {
    public:
        virtual ~CanonicalOptions() = default;

        GuestMemory memory;
        Encoding string_encoding;
        virtual void trap(const char *msg = "") = 0;
        virtual int realloc(int ptr, int old_size, int align, int new_size) = 0;
        virtual std::pair<char8_t *, size_t> convert(char8_t *dest, const char8_t *src, uint32_t byte_len, Encoding from_encoding, Encoding to_encoding) = 0;
        virtual void post_return() = 0;
        bool sync = true;
        // std::optional<std::function<void>> callback;
        bool always_task_return = false;
    };
    using CanonicalOptionsPtr = std::shared_ptr<CanonicalOptions>;
    CanonicalOptionsPtr createCanonicalOptions(const GuestMemory &memory,
                                               const GuestRealloc &realloc,
                                               HostTrap trap,
                                               HostUnicodeConversion convert,
                                               Encoding encoding,
                                               const GuestPostReturn &post_return);

    class LiftLowerContext
    {
    public:
        virtual ~LiftLowerContext() = default;

        CanonicalOptionsPtr opts;
    };
    using CallContextPtr = std::shared_ptr<LiftLowerContext>;
    CallContextPtr createCallContext(const GuestMemory &memory,
                                     const GuestRealloc &realloc,
                                     HostTrap trap,
                                     HostUnicodeConversion convert,
                                     Encoding encoding = Encoding::Utf8,
                                     const GuestPostReturn &post_return = nullptr);
}

#endif