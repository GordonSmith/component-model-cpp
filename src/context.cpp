#include "context.hpp"

#include <cstring>
#include <optional> // Include the necessary header file
#include <cassert>

namespace cmcpp
{
    class CanonicalOptionsImpl : public CanonicalOptions
    {
    private:
        const GuestRealloc &_realloc;
        const GuestPostReturn &_post_return;
        HostTrap _trap;
        HostUnicodeConversion _convert;

    public:
        CanonicalOptionsImpl(const GuestMemory &memory,
                             Encoding string_encoding,
                             const GuestRealloc &realloc,
                             HostTrap trap,
                             HostUnicodeConversion convert,
                             const GuestPostReturn &post_return) : _realloc(realloc), _trap(trap), _convert(convert), _post_return(post_return)
        {
            this->memory = memory;
            this->string_encoding = string_encoding;
        }

        virtual void trap(const char *msg = "")
        {
            //  Optional
            _trap(msg);
        }

        virtual int realloc(int ptr, int old_size, int align, int new_size)
        {
            return _realloc(ptr, old_size, align, new_size);
        }

        virtual std::pair<char8_t *, size_t> convert(char8_t *dest, const char8_t *src, uint32_t byte_len, Encoding from_encoding, Encoding to_encoding)
        {
            return _convert(dest, src, byte_len, from_encoding, to_encoding);
        }

        void post_return()
        {
            //  Optional
            if (_post_return)
            {
                _post_return();
            }
        }
    };

    CanonicalOptionsPtr createCanonicalOptions(const GuestMemory &memory,
                                               const GuestRealloc &realloc,
                                               HostTrap trap,
                                               HostUnicodeConversion convert,
                                               Encoding encoding,
                                               const GuestPostReturn &post_return)
    {
        return std::make_shared<CanonicalOptionsImpl>(memory, encoding, realloc, trap, convert, post_return);
    }

    class CallContextImpl : public LiftLowerContext
    {
    public:
        CallContextImpl(CanonicalOptionsPtr options) { opts = options; }
    };

    CallContextPtr createCallContext(const GuestMemory &memory,
                                     const GuestRealloc &realloc,
                                     HostTrap trap,
                                     HostUnicodeConversion convert,
                                     Encoding encoding,
                                     const GuestPostReturn &post_return)
    {
        return std::make_shared<CallContextImpl>(
            createCanonicalOptions(memory, realloc, trap, convert, encoding, post_return));
    }

}